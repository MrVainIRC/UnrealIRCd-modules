/*
 * Project Name: AwayEcho
 *
 * Description:
 * This module extends UnrealIRCd to send away-notify messages
 * back to the client that triggered them (in addition to the usual recipients),
 * if the client has negotiated the "away-notify" capability.
 * This can be useful for clients or scripts that rely on receiving their
 * own AWAY status changes via standard protocol messages.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Links to external resources:
 * - UnrealIRCd module development docs: https://www.unrealircd.org/docs/Modules
 * - IRCv3 specs including away-notify: https://ircv3.net/specs/extensions/away-notify
 *
 * - Source maintained by MrVain
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"third/awayecho",
	"0.1.0",
	"Echoes away-notify to the sender as well (if CAP active)",
	"Mrvain",
	"unrealircd-6"
};

int my_away_hook(Client *client, MessageTag *mtags, const char *reason, int already_as_away);

MOD_INIT() {
	return MOD_SUCCESS;
}

MOD_LOAD() {
	HookAdd(modinfo->handle, HOOKTYPE_AWAY, 0, my_away_hook);
	return MOD_SUCCESS;
}

MOD_UNLOAD() {
	return MOD_SUCCESS;
}

int my_away_hook(Client *client, MessageTag *mtags, const char *reason, int already_as_away)
{
	if (!HasCapability(client, "away-notify"))
		return 0;

	if (reason && *reason) {
		sendto_one(client, mtags, ":%s AWAY :%s", client->name, reason);
	
	} else {
		sendto_one(client, mtags, ":%s AWAY", client->name);
	}

	return 0;
}