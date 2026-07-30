# metadata-2-local Module for UnrealIRCd

**NOTE!** This module is a modified version of [metadata-2.c](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/metadata-2.c).

It adds IRCv3 `draft/metadata-2` support while avoiding blind network-wide metadata propagation. Local clients can use `METADATA` normally. Linked servers only receive metadata updates after they have first sent valid S2S `METADATA`, which marks them as metadata-capable for the current module runtime.

- **IRCv3 Metadata Support**: Adds the `draft/metadata-2` and `draft/metadata-notify-2` capabilities and the `METADATA` command.
- **Local-First Operation**: Local clients can set, get, list, clear, subscribe to, and sync metadata.
- **Filtered S2S Propagation**: Metadata is sent only to linked servers that previously sent S2S `METADATA` to this server.
- **No Server Sync Burst**: The module does not send metadata during server sync and is not marked as a global module.
- **Live Metadata Only**: User metadata is bound to the active client connection. It is not stored after disconnect.
- **Channel Lifetime Cleanup**: Channel metadata is bound to the live channel object and is freed when the channel is destroyed.
- **Abuse Limits**: Metadata key counts, subscription counts, and value size remain configurable.

## Requirements

- UnrealIRCd 6
- IRCv3-capable clients for best results
- The `batch` capability should be available for proper metadata batches

**Important**: Do NOT load `metadata-2-local` together with other `METADATA` modules.

## Configuration

Load the module and configure the metadata limits in `unrealircd.conf`:

```conf
loadmodule "third/metadata-2-local";

metadata {
    max-user-metadata 10;
    max-channel-metadata 10;
    max-subscriptions 10;
    max-value-bytes 300;
};
```

### Settings

- **`max-user-metadata`** (optional, default: `10`)  
  Sets how many metadata keys a user may store on themselves.

- **`max-channel-metadata`** (optional, default: `10`)  
  Sets how many metadata keys may be stored per channel. A value of `0` disables new channel metadata.

- **`max-subscriptions`** (optional, default: `10`)  
  Sets how many metadata keys a client may subscribe to for automatic updates.

- **`max-value-bytes`** (optional, default: `300`)  
  Sets the maximum metadata value length in bytes. Values must also be valid UTF-8.

There is intentionally no `hold-time` or `restore-delay`. The module does not retain metadata for offline users, accounts, or nicknames.

## Client Usage

Examples:

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

## Server-to-Server Behavior

This module deliberately differs from the original global behavior:

- It accepts incoming S2S `METADATA`.
- A server that sends S2S `METADATA` is remembered as metadata-capable until module unload/reload.
- Later metadata updates are sent only to remembered metadata-capable servers.
- Updates are not echoed back to the server they came from.
- Unknown linked servers receive no metadata updates.
- No metadata burst is sent during server sync.

This makes the module suitable for networks where only some servers understand metadata.

## Notes

- User metadata disappears when the client disconnects.
- Channel metadata disappears when the channel is destroyed.
- Remote user metadata is only as complete as the remote server's S2S metadata support.
- The metadata-capable server list is in memory only and is reset on module unload/reload.

## Troubleshooting

1. Do not load `metadata-2-local` with other `METADATA` modules at the same time.
2. Make sure clients request `draft/metadata-2` to use metadata features.
3. Check `max-user-metadata`, `max-channel-metadata`, and `max-value-bytes` if setting metadata fails.
4. Check channel privileges if channel metadata fails.
5. Test on a non-live server before deploying widely.
6. Check UnrealIRCd logs for config or module loading errors.

## Useful Links

- [UnrealIRCd Documentation](https://www.unrealircd.org/docs/)
- [IRCv3 Metadata Specification](https://ircv3.net/specs/extensions/metadata)
- [IRCv3 Specifications on GitHub](https://github.com/ircv3/ircv3-specifications)
- [Original metadata-2.c](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/metadata-2.c)

## License

This project is under the GPL Version 3 License. See the [LICENSE](LICENSE) file for details.

## Thanks for Using metadata-2-local

This module is a simple way to share server statistics on UnrealIRCd.

If you have suggestions or spot any issues, feel free to open an issue or submit a pull request. I’m always open to suggestions and learning from others!

I’m just a beginner in programming, and these module are a work in progress. So, use them at your own risk! I try to make sure everything works, but if something goes wrong, I can’t take responsibility for it. Make backups and test in a safe environment first!