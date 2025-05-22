
# AwayEcho Module for UnrealIRCd

This module extends your UnrealIRCd server to echo away-notify messages **back to the client who triggered them**. Normally, IRC servers only send away-notify messages to other clients in the same channel. With AwayEcho, your client will receive the same `AWAY` or `AWAY :message` line it would get from another user -- making scripting and automation easier.

- **Echoes your own AWAY status**: Get a protocol-level message when you mark yourself as away or come back.
- **Standards-compliant base**: Only sends echo if your client has negotiated the `away-notify` capability (per [IRCv3 spec](https://ircv3.net/specs/extensions/away-notify)).

## Requirements

- UnrealIRCd 6.x

This module has no external dependencies and works out of the box with UnrealIRCd 6.

## Installation

1. Copy `awayecho.c` into your UnrealIRCd source tree:
   ```bash
   cp away_echo.c /path/to/unrealircd/src/modules/third/
   ```

2. Compile the module:
   ```bash
   cd /path/to/unrealircd
   make custommodule module=src/modules/third/awayecho.c
   ```

3. Load the module in your `unrealircd.conf`:
   ```conf
   loadmodule "third/awayecho";
   ```

4. Rehash or restart your UnrealIRCd server:
   ```
   /REHASH
   ```

## How It Works

This module hooks into UnrealIRCd's `HOOKTYPE_AWAY`. Whenever a user marks themselves away or comes back:
- If the client has negotiated `away-notify`
- Then they will also receive the same `AWAY` or `AWAY :reason` line
- Exactly like other users in the same channel already do

### Example

When a user sends `/AWAY I'm not here`, they will receive:

```irc
:YourNick AWAY :I'm not here
```

And when they come back with `/AWAY`, they will receive:

```irc
:YourNick AWAY
```

## Use Cases

- **Scripting / automation**: For bots or clients that need to detect their own away status reliably.
- **Client debugging**: Easier to see that away-notify works correctly.
- **UI integration**: Clients can treat all away notifications (including self) identically.

## Troubleshooting Tips

1. **Make sure your client requests `away-notify`**. You won't see anything otherwise.
2. **The module only sends to clients with the cap** -- this prevents protocol violations.
3. **Still nothing?** Check your `unrealircd.log` for module load errors or recompile issues.

## Useful Links

- [UnrealIRCd Documentation](https://www.unrealircd.org/docs/)
- [IRCv3 away-notify specification](https://ircv3.net/specs/extensions/away-notify)
- [UnrealIRCd Modules Overview](https://www.unrealircd.org/docs/Modules)

## License

This project is licensed under the **AGPLv3**. See [LICENSE](https://www.gnu.org/licenses/agpl-3.0.en.html) for details.

## Thanks for Using AwayEcho!

This module is intended to make client development and scripting easier by unifying away-notify handling. If you find bugs or improvements, feel free to fork and contribute!

If you know how to improve these module or if you find a bug, feel free to open an issue or submit a pull request. I’m always open to suggestions and learning from others!

I’m just a beginner in programming, and these module are a work in progress. So, use it at your own risk! I try to make sure everything works, but if something goes wrong, I can’t take responsibility for it. Make backups and test in a safe environment first!
