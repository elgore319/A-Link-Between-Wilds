# Changelog

All notable changes to this project. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Protocol version changes are always called out.

## [Unreleased]

### Added
- Wire protocol v1 shared between client and server (`protocol/albw_protocol.h`, `server/protocol.py`).
- UDP relay server with join/reject, state relay, stale-packet dropping, ping/pong, timeouts and 14 tests.
- `tools/fake_client.py` test player.
- Switch module built on exlaunch (upstream `f9f4b0dd`): network thread, server join, 20 Hz state stream, test pattern until game offsets are known, inline player-update hook ready for offsets.
- CI: server tests, protocol layout check, Switch module build with downloadable artifacts.
- Docs: README, protocol spec, offset-finding guide, research log, roadmap, decision records 0001–0005.
