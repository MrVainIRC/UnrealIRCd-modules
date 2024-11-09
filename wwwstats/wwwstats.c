/* Copyright (C) All Rights Reserved
** Written by rocket & k4be modified by MrVain
** Website: https://github.com/pirc-pl/unrealircd-modules/
** License: GPLv3 https://www.gnu.org/licenses/gpl-3.0.html
*/

/*** <<<MODULE MANAGER START>>>
module
{
        documentation "https://github.com/pirc-pl/unrealircd-modules/blob/master/README.md";
        troubleshooting "In case of problems, contact MrVain on irc.retronode.org.";
        min-unrealircd-version "6.*";
        post-install-text {
                "The module is installed. Now you need to add a loadmodule line:";
                "loadmodule \"third/wwwstats\";";
                "then create a valid configuration block as in the example below:";
                "wwwstats {";
				" socket-path \"/tmp/wwwstats.sock\"; // this option is REQUIRED";
				"};";
				"And /REHASH the IRCd.";
				"";
				"If you want a version with MySQL/MariaDB support, look for 'wwwstats-mysql'";
				"on https://github.com/pirc-pl/unrealircd-modules - unfortunately it can't";
				"be installed by the module manager.";
				"Detailed documentation is available on https://github.com/pirc-pl/unrealircd-modules/blob/master/README.md#wwwstats";
        }
}
*** <<<MODULE MANAGER END>>>
*/

#define MYCONF "wwwstats"

#include "unrealircd.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <stdio.h>

#ifndef TOPICLEN
#define TOPICLEN MAXTOPICLEN
#endif

#if (UNREAL_VERSION_GENERATION == 5 && UNREAL_VERSION_MAJOR == 0 && UNREAL_VERSION_MINOR < 5)
#define MESSAGE_SENDTYPE int
#else
#define MESSAGE_SENDTYPE SendType
#endif

#define CHANNEL_MESSAGE_COUNT(channel) moddata_channel(channel, message_count_md).i

int counter;
time_t init_time;

int stats_socket;
char send_buf[4096];
struct sockaddr_un stats_addr;
ModDataInfo *message_count_md;

int wwwstats_msg(Client *sptr, Channel *chptr, MessageTag **mtags, const char *msg, MESSAGE_SENDTYPE sendtype);

EVENT(wwwstats_socket_evt);
char *json_escape(char *d, const char *a);
void md_free(ModData *md);
int wwwstats_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int wwwstats_configposttest(int *errs);
int wwwstats_configrun(ConfigFile *cf, ConfigEntry *ce, int type);

// config file stuff
static char *socket_path;
int socket_hpath = 0;
static char **selected_nicks = NULL;
static int num_nicks = 0;

static void parse_nick_list(const char *nicks_str) {
    char *nicks_copy, *nick, *saveptr;
    nicks_copy = strdup(nicks_str);

    num_nicks = 1;
    for (char *c = nicks_copy; *c; c++) {
        if (*c == ',') {
            num_nicks++;
        }
    }

    selected_nicks = safe_alloc(sizeof(char *) * num_nicks);

    int i = 0;
    nick = strtok_r(nicks_copy, ",", &saveptr);
    while (nick != NULL && i < num_nicks) {
        while (*nick == ' ') nick++;
        selected_nicks[i++] = strdup(nick);
        nick = strtok_r(NULL, ",", &saveptr);
    }

    free(nicks_copy);
}

int wwwstats_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
    ConfigEntry *cep;
    int errors = 0;

    if(type != CONFIG_MAIN)
        return 0;

    if(!ce || !ce->name)
        return 0;

    if(strcmp(ce->name, MYCONF))
        return 0;

    for(cep = ce->items; cep; cep = cep->next) {
        if(!cep->name) {
            config_error("%s:%i: blank %s item", cep->file->filename, cep->line_number, MYCONF);
            errors++;
            continue;
        }

        if(!strcmp(cep->name, "socket-path")) {
            if(!cep->value) {
                config_error("%s:%i: %s::%s must be a path", cep->file->filename, cep->line_number, MYCONF, cep->name);
                errors++;
                continue;
            }
            socket_hpath = 1;
            continue;
        }

        if(!strcmp(cep->name, "nicks")) {
            if(!cep->value) {
                config_error("%s:%i: %s::%s must be a list of nicks", cep->file->filename, cep->line_number, MYCONF, cep->name);
                errors++;
                continue;
            }
            parse_nick_list(cep->value);
            continue;
        }

        config_warn("%s:%i: unknown item %s::%s", cep->file->filename, cep->line_number, MYCONF, cep->name);
    }

    *errs = errors;
    return errors ? -1 : 1;
}

int wwwstats_configposttest(int *errs) {
    if(!socket_hpath){
        config_warn("[wwwstats] warning: socket path not specified! Socket won't be created. This module will not be useful.");
    }
    if(num_nicks == 0) {
        config_warn("[wwwstats] warning: no nicks specified! No nick status will be checked.");
    }
    return 1;
}

int wwwstats_configrun(ConfigFile *cf, ConfigEntry *ce, int type) {
    ConfigEntry *cep;

    if(type != CONFIG_MAIN)
        return 0;

    if(!ce || !ce->name)
        return 0;

    if(strcmp(ce->name, MYCONF))
        return 0;

    for(cep = ce->items; cep; cep = cep->next) {
        if(cep->value && !strcmp(cep->name, "socket-path")) {
            socket_path = strdup(cep->value);
            continue;
        }
        if(cep->value && !strcmp(cep->name, "nicks")) {
            continue;
        }
    }
    return 1;
}

ModuleHeader MOD_HEADER = {
    "third/wwwstats",
    "0.1.0",
    "Provides data for network stats including configurable nick status via Unix socket",
    "rocket, k4be, MrVain",
    "unrealircd-6"
};

MOD_TEST() {
    socket_hpath = 0;
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, wwwstats_configtest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, wwwstats_configposttest);
    return MOD_SUCCESS;
}

MOD_INIT() {
    ModDataInfo mreq;
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, wwwstats_configrun);
    HookAdd(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, wwwstats_msg);

    memset(&mreq, 0, sizeof(mreq));
    mreq.type = MODDATATYPE_CHANNEL;
    mreq.name = "message_count";
    mreq.free = md_free;
    message_count_md = ModDataAdd(modinfo->handle, mreq);
    if(!message_count_md){
        config_error("[%s] Failed to request message_count moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
        return MOD_FAILED;
    }

    return MOD_SUCCESS;
}

MOD_LOAD() {
    if(socket_path){
        stats_addr.sun_family = AF_UNIX;
        strcpy(stats_addr.sun_path, socket_path);
        unlink(stats_addr.sun_path);
    }

    counter = 0;

    if(socket_path){
        stats_socket = socket(PF_UNIX, SOCK_STREAM, 0);
        bind(stats_socket, (struct sockaddr*) &stats_addr, SUN_LEN(&stats_addr));
        chmod(stats_addr.sun_path, 0777);
        listen(stats_socket, 5);
        fcntl(stats_socket, F_SETFL, O_NONBLOCK);
    }

    EventAdd(modinfo->handle, "wwwstats_socket", wwwstats_socket_evt, NULL, 100, 0);

    return MOD_SUCCESS;
}

MOD_UNLOAD() {
    close(stats_socket);
    unlink(stats_addr.sun_path);

    if(socket_path) free(socket_path);

    if (selected_nicks) {
        for (int i = 0; i < num_nicks; i++) {
            free(selected_nicks[i]);
        }
        free(selected_nicks);
    }

    return MOD_SUCCESS;
}

void md_free(ModData *md) {
    md->i = 0;
}

int wwwstats_msg(Client *sptr, Channel *chptr, MessageTag **mtags, const char *msg, MESSAGE_SENDTYPE sendtype) {
    counter++;
    CHANNEL_MESSAGE_COUNT(chptr)++;
    return HOOK_CONTINUE;
}

EVENT(wwwstats_socket_evt) {
    char topic[6*TOPICLEN+1];
    char name[6*CHANNELLEN+1];
    int sock;
    struct sockaddr_un cli_addr;
    socklen_t slen;
    Client *acptr;
    Channel *channel;
    unsigned int hashnum;
    json_t *output = NULL;
    json_t *servers = NULL;
    json_t *channels = NULL;
    json_t *server_j = NULL;
    json_t *channel_j = NULL;
    json_t *nicks_status = NULL;
    char *result;

    if(!socket_hpath) return;

    sock = accept(stats_socket, (struct sockaddr*) &cli_addr, &slen);

    slen = sizeof(cli_addr);

    if(sock < 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN) return;
        unreal_log(ULOG_ERROR, "wwwstats", "WWWSTATS_ACCEPT_ERROR", NULL, "Socket accept error: $error", log_data_string("error", strerror(errno)));
        return;
    }

    output = json_object();
    servers = json_array();
    channels = json_array();
    nicks_status = json_array();

    json_object_set_new(output, "clients", json_integer(irccounts.clients));
    json_object_set_new(output, "channels", json_integer(irccounts.channels));
    json_object_set_new(output, "operators", json_integer(irccounts.operators));
    json_object_set_new(output, "servers", json_integer(irccounts.servers));
    json_object_set_new(output, "messages", json_integer(counter));

    list_for_each_entry(acptr, &global_server_list, client_node) {
        if (IsULine(acptr) && HIDE_ULINES)
            continue;

        server_j = json_object();
        json_object_set_new(server_j, "name", json_string_unreal(acptr->name));
        json_object_set_new(server_j, "users", json_integer(acptr->server->users));
        json_object_set_new(server_j, "uptime", json_integer((int)(TStime() - acptr->server->boottime)));

        json_array_append_new(servers, server_j);
    }
    json_object_set_new(output, "serv", servers);

    if (num_nicks > 0) {
        for(int i = 0; i < num_nicks; i++) {
            Client *nick = find_user(selected_nicks[i], NULL);
            json_t *nick_status = json_object();
            json_object_set_new(nick_status, "nick", json_string(selected_nicks[i]));
            json_object_set_new(nick_status, "online", nick ? json_true() : json_false());

            json_array_append_new(nicks_status, nick_status);
        }
    }
    json_object_set_new(output, "nicks_status", nicks_status);

    for(hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++) {
        for(channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch) {
            if(!PubChannel(channel)) continue;
            channel_j = json_object();
            json_object_set_new(channel_j, "name", json_string_unreal(channel->name));
            json_object_set_new(channel_j, "users", json_integer(channel->users));
            json_object_set_new(channel_j, "messages", json_integer(CHANNEL_MESSAGE_COUNT(channel)));
            if(channel->topic)
                json_object_set_new(channel_j, "topic", json_string_unreal(channel->topic));
            json_array_append_new(channels, channel_j);
        }
    }
    json_object_set_new(output, "chan", channels);
    result = json_dumps(output, JSON_COMPACT);

    char http_header[512];
    sprintf(http_header, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", strlen(result));

    send(sock, http_header, strlen(http_header), 0);
    send(sock, result, strlen(result), 0);

    json_decref(output);
    safe_free(result);
    close(sock);
}
