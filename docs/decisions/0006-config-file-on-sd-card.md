# 0006. Player settings from an INI file on the SD card

- **Date:** 2026-10-05
- **Status:** Accepted

## Context
Server address, port and player name were compile-time constants in `albw_config.hpp`. Every player would have needed devkitPro and a rebuild to join a different server, and testing between machines meant rebuilding for each IP. This was on the M2 list in the roadmap.

## Decision
- At startup the module reads `sd:/albw/config.ini`, mounted with `nn::fs::MountSdCardForDebug`. Ryujinx maps this to its `sdcard` folder; on a Switch it's the real SD card.
- Simple `key = value` INI with three keys (`server_ip`, `server_port`, `player_name`). Chosen over JSON because players will edit it in Notepad and a small hand-written parser needs no library.
- Compiled-in values remain as **defaults**. Any problem (no file, mount failure, bad line) falls back per setting and is logged; nothing in the file can stop the module from starting.
- The file is read on the network thread after its existing 5-second boot delay, not in `exl_main`, so file I/O can never stall the game's boot.
- The parser is a dependency-free header so it can be unit-tested on the host in CI. Everything SDK-specific stays in `config_file.cpp`.
- `UseConfigFile` in `albw_config.hpp` compiles the feature out entirely (no `nn::fs` symbols referenced).

## Consequences
- Players can change server/name without building. Build-only settings (send rate, test pattern) stay compiled in.
- **Unverified:** that BotW 1.6.0's SDK module exports `MountSdCardForDebug` and that the call succeeds on Ryujinx and under our `main.npdm` filesystem permissions on real hardware. If it fails, the module logs it and uses defaults; if the symbol is missing entirely, the module may fail to load, and `UseConfigFile = false` is the workaround until we switch to a different mount call. Record the result of the first in-game test in `docs/test-reports/` (added in PR #2).
- Hostnames aren't supported (would need DNS via `sfdnsres`); revisit if people want to share a domain instead of an IP.
