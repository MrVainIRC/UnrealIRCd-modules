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
                "loadmodule \"third/socketstats\";";
                "then create a valid configuration block as in the example below:";
                "socketstats {";
		"   socket-path \"/tmp/socketstats.sock\"; // this option is REQUIRED";
                "   nicks \"test1,test2\";"
                "   stream true|false|dynamic;"
                "   stream-interval 5;"
		"};";
		"And /REHASH the IRCd.";
		"";
        }
}
*** <<<MODULE MANAGER END>>>
*/

#define MYCONF "socketstats"

#include "unrealircd.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>

#if (UNREAL_VERSION_GENERATION == 5 && UNREAL_VERSION_MAJOR == 0 && UNREAL_VERSION_MINOR < 5)
#define MESSAGE_SENDTYPE int
#else
#define MESSAGE_SENDTYPE SendType
#endif

#define CHANNEL_MESSAGE_COUNT(channel) moddata_channel(channel, message_count_md).i

int counter;
int stats_socket;
struct sockaddr_un stats_addr;
ModDataInfo *message_count_md;

typedef struct stream_client {
    int sock;
} stream_client_t;

void socketstats_global_stream_evt(void *unused);
static stream_client_t **stream_clients = NULL;
static int stream_client_count = 0;
static Event *global_stream_event = NULL;

int socketstats_msg(Client *sptr, Channel *chptr, MessageTag **mtags, const char *msg, MESSAGE_SENDTYPE sendtype);

EVENT(socketstats_socket_evt);
void md_free(ModData *md);
int socketstats_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int socketstats_configposttest(int *errs);
int socketstats_configrun(ConfigFile *cf, ConfigEntry *ce, int type);

static char *socket_path;
int socket_hpath = 0;
static char **selected_nicks = NULL;
static int num_nicks = 0;
enum { STREAM_OFF, STREAM_ON, STREAM_DYNAMIC } stream_mode = STREAM_OFF;
static int streaming_interval = 10;

void get_cpu_info(double *total_usage, json_t *core_obj);
void get_ram_info(long *total_mb, long *used_mb);
void get_disk_info(long *total_mb, long *free_mb);
void get_ip_addresses(char *ipv4, size_t ipv4_len, char *ipv6, size_t ipv6_len);

static void parse_nick_list(const char *nicks_str) {
    if (!nicks_str || !*nicks_str) return;

    char *nicks_copy = strdup(nicks_str);
    if (!nicks_copy) return;

    num_nicks = 1;
    for (char *c = nicks_copy; *c; c++) {
        if (*c == ',') num_nicks++;
    }

    selected_nicks = safe_alloc(sizeof(char *) * num_nicks);
    if (!selected_nicks) {
        free(nicks_copy);
        return;
    }

    int i = 0;
    char *nick, *saveptr;
    nick = strtok_r(nicks_copy, ",", &saveptr);
    while (nick && i < num_nicks) {
        while (*nick == ' ') nick++;
        selected_nicks[i++] = strdup(nick);
        nick = strtok_r(NULL, ",", &saveptr);
    }

    free(nicks_copy);
}

int socketstats_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
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
        if (!strcmp(cep->name, "stream")) {
            if (cep->value) {
                if (!strcasecmp(cep->value, "true") || !strcasecmp(cep->value, "on") || !strcmp(cep->value, "1")) {
                    stream_mode = STREAM_ON;
                } else if (!strcasecmp(cep->value, "dynamic")) {
                    stream_mode = STREAM_DYNAMIC;
                } else if (!strcasecmp(cep->value, "false") || !strcasecmp(cep->value, "off") || !strcmp(cep->value, "0")) {
                    stream_mode = STREAM_OFF;
                } else {
                    config_warn("%s:%i: unknown value for %s::stream: %s", cep->file->filename, cep->line_number, MYCONF, cep->value);
                }
            }
            continue;
        }
        
        if (!strcmp(cep->name, "stream-interval")) {
            if (cep->value) {
                int val = atoi(cep->value);
                if (val > 0)
                    streaming_interval = val;
            }
            continue;
        }

        config_warn("%s:%i: unknown item %s::%s", cep->file->filename, cep->line_number, MYCONF, cep->name);
    }

    *errs = errors;
    return errors ? -1 : 1;
}

int socketstats_configposttest(int *errs) {
    if (!socket_hpath) {
        config_warn("[socketstats] warning: socket path not specified! Socket won't be created. This module will not be useful.");
    }

    if (num_nicks == 0) {
        config_warn("[socketstats] warning: no nicks specified! No nick status will be checked.");
    }

    switch (stream_mode) {
        case STREAM_OFF:
            config_warn("[socketstats] Streaming disabled.");
            break;
        case STREAM_ON:
            config_warn("[socketstats] Streaming enabled (interval: %d seconds).", streaming_interval);
            break;
        case STREAM_DYNAMIC:
            config_warn("[socketstats] Streaming in dynamic mode (interval: %d seconds).", streaming_interval);
            break;
    }

    return 1;
}

int socketstats_configrun(ConfigFile *cf, ConfigEntry *ce, int type) {
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
        
        if (cep->value && !strcmp(cep->name, "stream")) {
            continue;
        }
        
        if (cep->value && !strcmp(cep->name, "stream-interval")) {
            continue;
        }
    }
    return 1;
}

ModuleHeader MOD_HEADER = {
    "third/socketstats",
    "0.4.0",
    "Provides detailed server statistics via Unix socket, including server, client, channel, and operator counts, per-nick online status, system metrics (CPU, RAM, disk), host IP addresses and per-core CPU usage.",
    "rocket, k4be, MrVain",
    "unrealircd-6"
};

MOD_TEST() {
    socket_hpath = 0;
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, socketstats_configtest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, socketstats_configposttest);
    return MOD_SUCCESS;
}

MOD_INIT() {
    ModDataInfo mreq;
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, socketstats_configrun);
    HookAdd(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, socketstats_msg);

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
        if (bind(stats_socket, (struct sockaddr*)&stats_addr, SUN_LEN(&stats_addr)) < 0) {
            unreal_log(ULOG_ERROR, "socketstats", "BIND_ERROR", NULL,
                    "Bind failed: $error", log_data_string("error", strerror(errno)));
            return MOD_FAILED;
        }
        chmod(stats_addr.sun_path, 0777);
        if (listen(stats_socket, 5) < 0) {
            unreal_log(ULOG_ERROR, "socketstats", "LISTEN_ERROR", NULL,
                    "Listen failed: $error", log_data_string("error", strerror(errno)));
            return MOD_FAILED;
        }
        fcntl(stats_socket, F_SETFL, O_NONBLOCK);
    }

    EventAdd(modinfo->handle, "socketstats_socket", socketstats_socket_evt, NULL, 100, 0);

    return MOD_SUCCESS;
}

MOD_UNLOAD() {
    if (stats_socket >= 0) {
        close(stats_socket);
        stats_socket = -1;
    }

    if (socket_path) {
        unlink(stats_addr.sun_path);
        free(socket_path);
        socket_path = NULL;
    }

    if (selected_nicks) {
        for (int i = 0; i < num_nicks; i++) {
            if (selected_nicks[i])
                free(selected_nicks[i]);
        }
        free(selected_nicks);
        selected_nicks = NULL;
        num_nicks = 0;
    }

    return MOD_SUCCESS;
}

void md_free(ModData *md) {
    md->i = 0;
}

int socketstats_msg(Client *sptr, Channel *chptr, MessageTag **mtags, const char *msg, MESSAGE_SENDTYPE sendtype) {
    counter++;
    CHANNEL_MESSAGE_COUNT(chptr)++;
    return HOOK_CONTINUE;
}

json_t* build_json_output(void) {
    Client *acptr;
    Channel *channel;
    unsigned int hashnum;
    json_t *output = json_object();
    json_t *servers = json_array();
    json_t *channels = json_array();
    json_t *nicks_status = json_array();

    if (!output || !servers || !channels || !nicks_status)
        return NULL;

    json_object_set_new(output, "clients", json_integer(irccounts.clients));
    json_object_set_new(output, "channels", json_integer(irccounts.channels));
    json_object_set_new(output, "operators", json_integer(irccounts.operators));
    json_object_set_new(output, "messages", json_integer(counter));

    int server_count = 0;

    list_for_each_entry(acptr, &global_server_list, client_node) {
        if (!acptr->server) continue;

        server_count++;

        json_t *server_j = json_object();
        json_object_set_new(server_j, "name", json_string_unreal(acptr->name));
        json_object_set_new(server_j, "users", json_integer(acptr->server->users));
        json_object_set_new(server_j, "uptime", json_integer((int)(TStime() - acptr->server->boottime)));
        json_object_set_new(server_j, "is_uline", IsULine(acptr) ? json_true() : json_false());

        if (acptr == &me) {
            long ram_total, ram_used, disk_total, disk_free;
            char ip4[INET_ADDRSTRLEN] = "", ip6[INET6_ADDRSTRLEN] = "";

            get_ram_info(&ram_total, &ram_used);
            get_disk_info(&disk_total, &disk_free);
            get_ip_addresses(ip4, sizeof(ip4), ip6, sizeof(ip6));

            json_object_set_new(server_j, "is_local", json_true());
            json_object_set_new(server_j, "cpu_cores", json_integer(sysconf(_SC_NPROCESSORS_ONLN)));

            double cpu_usage_total;
            json_t *core_usages = json_object();
            get_cpu_info(&cpu_usage_total, core_usages);

            json_object_set_new(server_j, "cpu_usage_percent", json_real(cpu_usage_total));
            json_object_set_new(server_j, "cpu_core_usage_percent", core_usages);
            json_object_set_new(server_j, "ram_total_mb", json_integer(ram_total));
            json_object_set_new(server_j, "ram_used_mb", json_integer(ram_used));
            json_object_set_new(server_j, "disk_total_mb", json_integer(disk_total));
            json_object_set_new(server_j, "disk_free_mb", json_integer(disk_free));
            json_object_set_new(server_j, "host_ipv4", json_string(ip4));
            json_object_set_new(server_j, "host_ipv6", json_string(ip6));
        } else {
            json_object_set_new(server_j, "is_local", json_false());
        }

        json_array_append_new(servers, server_j);
    }

    json_object_set_new(output, "servers", json_integer(server_count));
    json_object_set_new(output, "serv", servers);

    if (num_nicks > 0) {
        for (int i = 0; i < num_nicks; i++) {
            Client *nick = find_user(selected_nicks[i], NULL);
            json_t *nick_status = json_object();
            json_object_set_new(nick_status, "nick", json_string(selected_nicks[i]));
            json_object_set_new(nick_status, "online", nick ? json_true() : json_false());
            json_array_append_new(nicks_status, nick_status);
        }
    }

    json_object_set_new(output, "nicks_status", nicks_status);

    for (hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++) {
        for (channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch) {
            if (!PubChannel(channel)) continue;
            json_t *channel_j = json_object();
            json_object_set_new(channel_j, "name", json_string_unreal(channel->name));
            json_object_set_new(channel_j, "users", json_integer(channel->users));
            json_object_set_new(channel_j, "messages", json_integer(CHANNEL_MESSAGE_COUNT(channel)));
            if (channel->topic)
                json_object_set_new(channel_j, "topic", json_string_unreal(channel->topic));
            json_array_append_new(channels, channel_j);
        }
    }

    json_object_set_new(output, "chan", channels);
    return output;
}

void add_stream_client(int sock) {
    stream_client_t **tmp = realloc(stream_clients, sizeof(stream_client_t *) * (stream_client_count + 1));
    if (!tmp) {
        close(sock);
        return; // Optional: Logmeldung hinzufügen
    }
    stream_clients = tmp;
    stream_clients[stream_client_count] = safe_alloc(sizeof(stream_client_t));
    stream_clients[stream_client_count]->sock = sock;
    stream_client_count++;

    if (!global_stream_event) {
        global_stream_event = EventAdd(NULL, "socketstats_global_stream_evt", socketstats_global_stream_evt, NULL, streaming_interval * 1000, 1);
    }
}

void remove_stream_client(int index) {
    close(stream_clients[index]->sock);
    safe_free(stream_clients[index]);

    for (int i = index; i < stream_client_count - 1; i++)
        stream_clients[i] = stream_clients[i + 1];

    stream_client_count--;

    if (stream_client_count == 0 && global_stream_event) {
        EventDel(global_stream_event);
        global_stream_event = NULL;
    }
}

void socketstats_global_stream_evt(void *unused) {
    json_t *output = build_json_output();
    if (!output) return;

    char *result = json_dumps(output, JSON_COMPACT);
    json_decref(output);
    if (!result) return;

    size_t full_len = strlen(result) + 2;
    char *full = safe_alloc(full_len);
    snprintf(full, full_len, "%s\n", result);
    safe_free(result);

    char chunk_size[32];
    snprintf(chunk_size, sizeof(chunk_size), "%zx\r\n", strlen(full));

    for (int i = 0; i < stream_client_count; i++) {
        int sock = stream_clients[i]->sock;
        if (send(sock, chunk_size, strlen(chunk_size), 0) <= 0 ||
            send(sock, full, strlen(full), 0) <= 0 ||
            send(sock, "\r\n", 2, 0) <= 0) {
            remove_stream_client(i--);
        }
    }

        safe_free(full);

    // Weiterplanen
    if (stream_client_count > 0) {
        global_stream_event = EventAdd(NULL, "socketstats_global_stream_evt", socketstats_global_stream_evt, NULL, streaming_interval * 1000, 1);
    }
}

EVENT(socketstats_socket_evt) {
    struct sockaddr_un cli_addr;
    socklen_t slen = sizeof(cli_addr);
    int sock;

    if (!socket_hpath)
        return;

    sock = accept(stats_socket, (struct sockaddr*)&cli_addr, &slen);
    if (sock < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return;
        unreal_log(ULOG_ERROR, "socketstats", "SOCKETSTATS_ACCEPT_ERROR", NULL,
                   "Socket accept error: $error", log_data_string("error", strerror(errno)));
        return;
    }

    // === Header empfangen ===
    char buffer[2048] = {0};
    ssize_t total = 0;
    while (total < sizeof(buffer) - 1) {
        ssize_t r = recv(sock, buffer + total, sizeof(buffer) - 1 - total, 0);
        if (r <= 0)
            break;
        total += r;
        if (strstr(buffer, "\r\n\r\n"))
            break;
    }
    buffer[total] = '\0';

    int client_wants_once = (strstr(buffer, "X-Once: true") != NULL);

    bool do_stream = false;
    if (stream_mode == STREAM_ON)
        do_stream = true;
    else if (stream_mode == STREAM_DYNAMIC && !client_wants_once)
        do_stream = true;

    // === STREAMING ===
    if (do_stream) {
        const char *header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n";
        send(sock, header, strlen(header), 0);

        add_stream_client(sock);
        return;
    }

    // === NON-STREAMING ===
    json_t *output = build_json_output();
    if (!output) {
        close(sock);
        return;
    }

    char *result = json_dumps(output, JSON_COMPACT);
    json_decref(output);

    if (result) {
        char http_header[512];
        snprintf(http_header, sizeof(http_header),
                 "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
                 strlen(result));

        if (send(sock, http_header, strlen(http_header), 0) > 0)
            send(sock, result, strlen(result), 0);

        safe_free(result);
    }

    close(sock);
}

void get_cpu_info(double *total_usage, json_t *core_obj) {
    static unsigned long long prev_total[129] = {0}, prev_idle[129] = {0}; 
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        *total_usage = 0.0;
        return;
    }

    char line[256];
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    *total_usage = 0.0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) != 0 || (!isspace(line[3]) && !isdigit(line[3])))
            break;

        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        int index;

        if (isspace(line[3])) {
            index = 0;
            sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        } else {
            int parsed_core;
            sscanf(line, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu",
                   &parsed_core, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            if (parsed_core >= cpu_count)
                break;
            index = parsed_core + 1;
        }

        unsigned long long idle_time = idle + iowait;
        unsigned long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;

        unsigned long long delta_idle = idle_time - prev_idle[index];
        unsigned long long delta_total = total_time - prev_total[index];

        double usage = (delta_total > 0) ? (1.0 - (double)delta_idle / delta_total) * 100.0 : 0.0;

        prev_idle[index] = idle_time;
        prev_total[index] = total_time;

        if (index == 0) {
            *total_usage = usage;
        } else {
            char key[16];
            snprintf(key, sizeof(key), "core%d", index - 1);
            json_object_set_new(core_obj, key, json_real(usage));
        }
    }

    fclose(fp);
}

void get_ram_info(long *total_mb, long *used_mb) {
    struct sysinfo info;
    sysinfo(&info);
    *total_mb = info.totalram / 1024 / 1024;
    *used_mb = (info.totalram - info.freeram) / 1024 / 1024;
}

void get_disk_info(long *total_mb, long *free_mb) {
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        *total_mb = (stat.f_blocks * stat.f_frsize) / 1024 / 1024;
        *free_mb = (stat.f_bfree * stat.f_frsize) / 1024 / 1024;
    } else {
        *total_mb = *free_mb = 0;
    }
}

void get_ip_addresses(char *ipv4, size_t ipv4_len, char *ipv6, size_t ipv6_len) {
    struct ifaddrs *ifaddr, *ifa;
    getifaddrs(&ifaddr);
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || !(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK))
            continue;

        int family = ifa->ifa_addr->sa_family;
        void *addr;
        char ipstr[INET6_ADDRSTRLEN];

        if (family == AF_INET && ipv4[0] == '\0') {
            addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(family, addr, ipstr, sizeof(ipstr));
            if (strncmp(ipstr, "192.168.", 8) && strncmp(ipstr, "10.", 3) &&
                !(strncmp(ipstr, "172.", 4) == 0 && atoi(ipstr + 4) >= 16 && atoi(ipstr + 4) <= 31))
                strncpy(ipv4, ipstr, ipv4_len);
        } else if (family == AF_INET6 && ipv6[0] == '\0') {
            addr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            inet_ntop(family, addr, ipstr, sizeof(ipstr));
            if (strncmp(ipstr, "fe80", 4) && strncmp(ipstr, "::1", 3))
                strncpy(ipv6, ipstr, ipv6_len);
        }

        if (ipv4[0] && ipv6[0]) break;
    }
    freeifaddrs(ifaddr);
}
