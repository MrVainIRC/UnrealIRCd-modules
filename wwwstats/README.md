# wwwstats Module for UnrealIRCd

**NOTE!** This module is a modified version of [wwwstats](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/wwwstats.c).

This module enables your UnrealIRCd server to share statistics with a web-based system through a UNIX socket, providing real-time insights on server activity.

- **Web-Friendly Output**: Outputs statistics in JSON format, suitable for generating live channel and server lists, displaying user counts, and more.
- **Improved Proxy Handling**: Includes an HTTP/1.1 header for easier integration with proxies.
- **Online Status for Nicknames**: Allows checking the online status of specific nicknames, making it more flexible for use with frontend applications.

## Requirements

- UnrealIRCd 6

**Important**: Do NOT install together with [wwwstats](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/wwwstats.c) or [wwwstats-mysql](https://github.com/pirc-pl/unrealircd-modules/blob/master/unreal6/wwwstats-mysql.c).

## Known Limitations

- **Message Counters**: Counts only messages passing through the server where the module is loaded, so it may not capture all messages across networked servers.
- **Ignored Channels**: Private and secret channels (+p / +s) are always excluded from the statistics.

## Configuration

Once wwwstats is installed, set it up in your `unrealircd.conf`. Here’s an example:

```conf
loadmodule "third/wwwstats";

wwwstats {
    socket-path "/tmp/wwwstats.sock";
    nicks "nick1, nick2, nick3"
};
```

### How It Works

- **socket-path**: Required option to specify where the UNIX socket will be created.
- **nicks**: Allows querying the online status of specific nicknames

## Testing It Out

After configuration, you can test the socket output with the following shell command:
```
socat - UNIX-CONNECT:/tmp/wwwstats.sock
```
You’ll receive a JSON response containing live server data, similar to the example below.
```json
{
    "clients": 19,
    "channels": 4,
    "operators": 18,
    "servers": 2,
    "messages": 1459,
    "nicks"
    "serv": [{
        "name": "test1.example.com",
        "users": 2
    }],
    "nicks-status": [{
        "nick": "nick1",
        "online": true
    }, {
        "nick": "nick2",
        "online": false
    }],  
    "chan": [{
        "name": "#help",
        "users": 1,
        "messages": 0
    }, {
        "name": "#services",
        "users": 8,
        "messages": 971
    }, {
        "name": "#opers",
        "users": 1,
        "messages": 0,
        "topic": "This channel has some topic"
    }]
}
```

## Troubleshooting Tips

1. **Check your config**: Make sure `unrealircd.conf` is correctly set up, especially in the socket-path section.
2. **Test on a non-live server**: It’s always a good idea to test on a non-live server first.
3. **Check logs**: UnrealIRCd logs might have useful information if things aren’t working as expected.

## Useful Links

- [UnrealIRCd Documentation](https://www.unrealircd.org/docs/)
- [pirc-pl/unrealircd-modules](https://github.com/pirc-pl/unrealircd-modules/blob/master/README.md)

## License

This project is under the GPL Version 3 License. See the [LICENSE](LICENSE) file for details.

## Thanks for Using wwwstats!

This module is a simple way to share server statistics on UnrealIRCd.

If you have suggestions or spot any issues, feel free to open an issue or submit a pull request. I’m always open to suggestions and learning from others!

I’m just a beginner in programming, and these module are a work in progress. So, use them at your own risk! I try to make sure everything works, but if something goes wrong, I can’t take responsibility for it. Make backups and test in a safe environment first!
