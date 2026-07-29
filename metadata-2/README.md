# metadata-2-local Module for UnrealIRCd

**NOTE!** This module is a modified local-only version of [metadata-2](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/metadata-2.c).

This module enables IRCv3 `draft/metadata-2` support on your UnrealIRCd server while keeping outgoing effects local to the server where it is loaded. Local clients can set, read, subscribe to, and sync metadata without the module actively spamming linked servers with `METADATA` commands they may not understand.

- **IRCv3 Metadata Support**: Adds the `draft/metadata-2` capability and the `METADATA` command.
- **Local-First Behavior**: Metadata changes are delivered to local subscribed clients, but are not actively propagated to linked servers.
- **Incoming S2S Compatible**: If another server sends valid S2S `METADATA`, this module can accept it.
- **Configurable Limits**: Keeps limits for metadata keys, subscriptions, and value size to prevent abuse.
- **Hybrid Retention**: Keeps metadata by services account for logged-in users and by nickname for users who are not logged in.
- **Nick-Independent Account Restore**: Account metadata can be restored even if the user reconnects with a different nickname.

## Requirements

- UnrealIRCd 6
- IRCv3-capable clients for best results
- The `batch` capability should be available for proper metadata batches

**Important**: Do NOT install together with `metadata-3-local`. Both modules register the `METADATA` command.

## Known Limitations

- **Registered Means Services Account**: In this README, "registered" means logged into a services account, not just connected to IRC.
- **Local Retention Only**: `hold-time` is only intended for local users on the server where this module is loaded.
- **No Outgoing Network Sync**: This module does not actively send metadata updates or sync bursts to linked servers.
- **Remote Metadata Depends on Remote Support**: Remote users can only provide metadata if their server sends compatible S2S `METADATA`.
- **In-Memory Storage**: Retained metadata is kept in memory. It is lost on UnrealIRCd restart or module unload.
- **Account Wins Over Nick**: If the user is logged into a services account, retention uses that account. If not, retention uses the nickname.

## Configuration

Once `metadata-2-local` is installed, set it up in your `unrealircd.conf`. Here is an example:

```conf
loadmodule "third/metadata-2-local";

metadata {
    max-user-metadata 10;
    max-channel-metadata 10;
    max-subscriptions 10;
    max-value-bytes 300;
    hold-time 10d;
    restore-delay 30s;
};
```

### How It Works

- **`max-user-metadata`** (optional, default: `10`)  
  Sets how many metadata keys a user may store on their own nickname.

- **`max-channel-metadata`** (optional, default: `10`)  
  Sets how many metadata keys may be stored per channel. A value of `0` disables new channel metadata.

- **`max-subscriptions`** (optional, default: `10`)  
  Sets how many metadata keys a client may subscribe to for automatic updates.

- **`max-value-bytes`** (optional, default: `300`)  
  Sets the maximum value length in bytes. Values must also be valid UTF-8.

- **`hold-time`** (optional, default: `10d`)  
  Sets how long metadata is retained after disconnect. Logged-in users are retained by services account; users without an account are retained by nickname.
  - `0`: Delete immediately on disconnect.
  - `30m`: Keep for 30 minutes.
  - `12h`: Keep for 12 hours.
  - `10d`: Keep for 10 days.
  - `1w`: Keep for 1 week.

- **`restore-delay`** (optional, default: `30s`)  
  Delays nickname-based restore for users who are not logged into a services account yet.
  - `0`: Restore nickname metadata immediately.
  - `30s`: Wait 30 seconds before restoring nickname metadata.
  - `2m`: Wait 2 minutes before restoring nickname metadata.

  If the user logs into a services account during this delay, account metadata wins and the pending nickname restore is cancelled. If the user changes nick during this delay, the old pending restore is cancelled and a new one is scheduled for the new nick.

  Pending nickname restores are checked every 5 seconds, so the actual restore may happen a few seconds after the configured delay.

## What Clients Can Do

Clients can use the IRCv3 `METADATA` command, for example:

```irc
METADATA * SET status :Away for coffee
METADATA * GET status
METADATA * LIST
METADATA * SUB status display-name
METADATA * SYNC
METADATA * CLEAR
```

The server advertises the capability like this:

```irc
draft/metadata-2=before-connect,max-subs=10,max-keys=10,max-value-bytes=300
```

## Local and Server-to-Server Behavior

This module intentionally avoids outgoing S2S metadata propagation:

- It does not send metadata changes to all linked servers.
- It does not send metadata bursts during server sync.
- It does not mark itself as a global module.
- It still accepts incoming S2S `METADATA` if another server supports and sends it.

This makes it suitable for networks where only one server currently supports metadata.

## Troubleshooting Tips

1. **Check module conflicts**: Do not load `metadata-2-local` and `metadata-3-local` at the same time.
2. **Check CAP negotiation**: Clients must request `draft/metadata-2` to receive metadata notifications.
3. **Check limits**: If setting metadata fails, verify `max-user-metadata`, `max-channel-metadata`, and `max-value-bytes`.
4. **Check permissions**: Channel metadata requires sufficient channel privileges.
5. **Test on a non-live server**: This is still a custom module, so test before deploying widely.
6. **Check logs**: UnrealIRCd logs may show config or module loading issues.

## Useful Links

- [UnrealIRCd Documentation](https://www.unrealircd.org/docs/)
- [IRCv3 Metadata Specification](https://ircv3.net/specs/extensions/metadata)
- [IRCv3 Specifications on GitHub](https://github.com/ircv3/ircv3-specifications)
- [pirc-pl/unrealircd-modules](https://github.com/pirc-pl/unrealircd-modules/blob/master/README.md)

## License

This module follows the license terms of the original module and UnrealIRCd module ecosystem. Check the source header and your UnrealIRCd distribution license before redistribution.

## Thanks for Using metadata-2-local!

This module is meant for small or mixed-support UnrealIRCd networks where metadata should be useful locally without breaking or confusing servers that do not support it yet.

Use it at your own risk. Make backups, test in a safe environment first, and keep an eye on your logs after loading or rehashing.
