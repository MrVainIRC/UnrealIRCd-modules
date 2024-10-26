# NickCollator Module for UnrealIRCd

This module enables your UnrealIRCd server to handle nick names across various scripts and languages by using Unicode encoding for consistent character handling.

- **Better Nick Name Handling**: Blocks nick names based on your custom character mappings, even across character sets.
- **Custom Character Mappings**: Set up rules to treat specific characters as the same across different scripts and alphabets.

## Requirements

- UnrealIRCd 6

- To compile this module, you’ll need to install the [**ICU library**](https://github.com/unicode-org/icu). You can usually do this with a package manager:

  - **Debian/Ubuntu**:
  ```bash
  sudo apt-get install libicu-dev
  ```

  - **CentOS/RHEL**:
  ```bash
  sudo yum install libicu-devel
  ```

**Important**: Make sure to adjust the Makefile to include the ICU libraries. For more information, check out the [UnrealIRCd Documentation](https://www.unrealircd.org/docs/) on compiling modules and the [ICU Project Documentation](https://unicode-org.github.io/icu/).

Also, for the NickCollator module to work properly across your network, you’ll need to install it on **all servers in the network**.

## Configuration

Once NickCollator is installed, set up your mappings in `unrealircd.conf`. Here’s an example:

```conf
loadmodule "nickcollator"; // load the module

nickcollator {
    mapping {
        "Ü, ü, Ue, ue";  // Define mappings here
        "О, O"           // Cyrillic "O" and Latin "O"
        "T, t, Т, т";    // Cyrillic "Т, т" and Latin "T, t"
        "Л, л, L, l";    // ...
    }
};
```

### How It Works

- **mapping**: This is where you define characters that should be treated as the same. For example, Cyrillic `О` (which looks like Latin `O`) can be mapped to avoid confusion. You can add as many mappings as you need for the characters you want to handle.

## Testing It Out

After setting it up, NickCollator will block users from choosing nick names based on your mappings. If `Müller` is taken, no one else can use `Mueller` if `ü` is mapped to `ue`, and so on.

## Troubleshooting Tips

1. **Check your config**: Make sure `unrealircd.conf` is correctly set up, especially in the mappings section.
2. **Test on a non-live server**: It’s always a good idea to test on a non-live server first.
3. **Check logs**: UnrealIRCd logs might have useful information if things aren’t working as expected.

## Useful Links

- [UnrealIRCd Documentation](https://www.unrealircd.org/docs/)
- [ICU Project](https://icu.unicode.org/)
- [ICU Project on GitHub](https://github.com/unicode-org/icu)

## License

This project is under the AGPL Version 3 License. See the [LICENSE](LICENSE) file for details.

## Thanks for Using NickCollator!

This module is a simple way to add some extra nick name control across different character sets on UnrealIRCd.

If you know how to improve these module or if you find a bug, feel free to open an issue or submit a pull request. I’m always open to suggestions and learning from others!

I’m just a beginner in programming, and these module are a work in progress. So, use it at your own risk! I try to make sure everything works, but if something goes wrong, I can’t take responsibility for it. Make backups and test in a safe environment first!
