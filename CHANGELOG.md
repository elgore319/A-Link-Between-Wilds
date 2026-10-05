# Changelog

All notable changes to this project. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Protocol version changes are always called out.

## [Unreleased]

### Added
- Decision 0008: setup must be doable by players who have never modded anything (one installer + name + join code; one-click host). Added to CLAUDE.md, CONTRIBUTING and a new "Friend-ready setup" roadmap milestone.
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
