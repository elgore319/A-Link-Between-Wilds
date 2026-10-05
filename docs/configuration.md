# Configuration

The module reads its settings from **`albw/config.ini` on the SD card** when the game starts (about 5 seconds after boot, on the network thread). No rebuild is needed to change server or name.

A ready-to-copy file with comments is in [`client/sdcard/albw/config.ini`](../client/sdcard/albw/config.ini).

## Where the file goes

| Platform | Path |
|---|---|
| Ryujinx | `<Ryujinx data folder>\sdcard\albw\config.ini`. The data folder is `%APPDATA%\Ryujinx` for a normal install, or the `portable` folder next to `Ryujinx.exe` for a portable one (*File → Open Ryujinx Folder* opens it either way). |
| Switch (Atmosphère) | `SD:/albw/config.ini` |

## Keys

| Key | Default | Valid values |
|---|---|---|
| `server_ip` | `127.0.0.1` | IPv4 address in dotted form, e.g. `192.168.1.50`. No hostnames, no leading zeros (`010.0.0.1` is rejected because the SDK would read it as octal). |
| `server_port` | `55420` | `1`–`65535`. Must match the server's `--port`. |
| `player_name` | `Link` | 1–16 printable ASCII characters (spaces allowed). |

Defaults are compiled in from [`albw_config.hpp`](../client/source/program/albw_config.hpp).

## Format rules
- `key = value`, one per line. Whitespace around keys and values is ignored; keys are case-insensitive.
- Lines starting with `#` or `;` are comments. `[section]` headers are allowed and ignored.
- UTF-8 with or without BOM, LF or CRLF endings (Windows Notepad files work).
- If a key appears twice, the later line wins.
- Max file size 4 KB.

## When something's wrong
Nothing in the file can stop the module from starting. A missing file, unreadable SD card or bad line means the built-in default is used for that setting. With Ryujinx guest logs on, you'll see one of:

```
[albw] no sd:/albw/config.ini (0x...), using built-in settings
[albw] config.ini line 3: server_port must be a number from 1 to 65535 (ignored)
[albw] loaded sd:/albw/config.ini: 2 setting(s) applied, 1 line(s) ignored
[albw] settings: server 192.168.1.50:55420, name 'Linkle'
```

The last line is always printed and shows the settings actually in use.

## Turning it off
Set `UseConfigFile = false` in `albw_config.hpp` and rebuild. The module then never touches the filesystem and doesn't link against `nn::fs` at all. That's the escape hatch if SD card access turns out not to work on some setup (see [decision 0006](decisions/0006-config-file-on-sd-card.md)).

## For developers
The parser ([`config_parser.hpp`](../client/source/program/config/config_parser.hpp)) has no Switch dependencies and is unit-tested on the host:

```bash
cd client/tests
g++ -std=c++20 -Wall -Wextra -Werror -I../source -I../../protocol test_config_parser.cpp -o test_config_parser && ./test_config_parser
```

CI runs these tests (with AddressSanitizer/UBSan) as the *Module unit tests (host)* job. To add a setting: add a field to `Settings`, a branch in `Parse()`, a default in `albw_config.hpp` and `SetDefaults()`, tests, and a row in the table above.
