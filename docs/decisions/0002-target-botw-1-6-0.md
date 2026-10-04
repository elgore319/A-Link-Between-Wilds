# 0002. Target BotW 1.6.0 only

- **Date:** 2026-10-04
- **Status:** Accepted

## Context
Every game update moves functions and data, so every offset is version-specific. Supporting several versions multiplies the reverse engineering work.

## Decision
Support only Switch v1.6.0, the final update and the version most mods and community research target. Program ID `01007EF00011E000`.

## Consequences
- One set of offsets in `client/source/program/game/game_offsets.hpp`.
- Players on other versions must update. Later, the module should detect the version and refuse to run on a mismatch rather than crash (exlaunch's `version.hpp` supports this).
