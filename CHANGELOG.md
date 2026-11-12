# Changelog

All notable changes to this project. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Protocol version changes are always called out.

## [Unreleased]

### Changed
- **Target game is now BotW 1.0.0 with no update, not 1.6.0** (decision 0009, supersedes 0002). README, guides, research notes and the offsets log updated; 1.6.0 addresses are kept as porting references only. The 1.0.0 player offsets still have to be found (see `docs/finding-offsets.md` §4).
- `test-m1` reports the game version (Ryujinx's and the module's) and warns when an update is installed.

### Added
- First-test polish:
  - The module finds the `nn::socket` functions in the game at runtime, trying each SDK spelling, instead of importing guessed names (decision 0011). A missing or misnamed function now logs `[albw] networking off: ...` plus the names the game does export, and the game keeps running, where before it would have crashed with no log line. IP and port are converted by our own tested code. Host tests in `client/tests/test_elf_symbols.cpp`, run in CI.
  - Clearer network log: each step is logged, a hint after 10 unanswered HELLOs, readable reject reasons, `Send()` errors (rate-limited), and a `[albw] status:` heartbeat every 30 s.
  - `test-m1.ps1` pre-flight checks: switches a selected BotW title update to "No Update" and re-enables the mod if it's off in Ryujinx's mod manager (both asked first, with backups), copies the default `config.ini` to Ryujinx's SD card if there is none, and warns about other BotW code mods. Results go in the report.
  - `test-m1.ps1` shows `[albw]` lines live, reuses a server already on the port, checks the server actually started, finds Ryujinx logs next to a running `Ryujinx.exe` too, waits for the heartbeat after joining, and has new verdicts: `CRASH` (with the last `[albw]` line) and `NETWORK OFF`. `-Yes` answers its questions for you.
  - Server: a plain message when the port is already taken, and a "still waiting" reminder every minute while nobody is connected. Tests added.
  - CI: parses every `.ps1` with Windows PowerShell 5.1 and dry-runs `test-m1.ps1 -InstallOnly` against a fake Ryujinx folder.
- In-game player scan (decision 0010): the module searches memory for the pointer to Link's position and logs one `scan RESULT` line with the values for `game_offsets.hpp`, picked by watching which candidate moves while you run. Read-only, checked reads, low-priority thread; `RunPlayerScan` in `albw_config.hpp`. Double-click `tools/windows/find-link.bat` (`test-m1.ps1 -Scan`) to run it and get a report. Host tests in `client/tests/test_player_scan.cpp`, run in CI.
- Startup game version check: the module reads the main executable's build ID and only hooks or reads the game on 1.0.0; on other builds it logs why and sends the test pattern. Host unit tests in `client/tests/test_build_id.cpp`, run in CI.
- Research: `docs/research/decomp-mapping.md` maps the BotW decomp (1.5.0) to 1.6.0. Player position candidates via the `PlayerInfo` singleton (main+0x2CA1140 → +0x60 → matrix at +0x398) and M3 spawn functions, logged as 🔍 in the offsets log, with in-game verification steps.
- Module reads server IP, port and player name from `sd:/albw/config.ini` at startup, falling back to the compiled-in defaults per setting; example file in `client/sdcard/albw/`, docs in `docs/configuration.md`, decision 0006. Can be compiled out with `UseConfigFile = false`.
- Host-side unit tests for the config parser (`client/tests/`), run in CI with sanitizers.
- `tools/windows/test-m1.ps1` (+ `.bat` wrapper): one-command in-game test on Windows/Ryujinx that installs the build, enables guest logs, starts the server, watches logs and writes a report to `docs/test-reports/`.
- Wire protocol v1 shared between client and server (`protocol/albw_protocol.h`, `server/protocol.py`).
- UDP relay server with join/reject, state relay, stale-packet dropping, ping/pong, timeouts and 14 tests.
- `tools/fake_client.py` test player.
- Switch module built on exlaunch (upstream `f9f4b0dd`): network thread, server join, 20 Hz state stream, test pattern until game offsets are known, inline player-update hook ready for offsets.
- CI: server tests, protocol layout check, Switch module build with downloadable artifacts.
- Docs: README, protocol spec, offset-finding guide, research log, roadmap, decision records 0001–0005.
