# 0001. Use exlaunch for code injection

- **Date:** 2026-10-04
- **Status:** Accepted

## Context
We need to run our own code inside BotW on Switch: read the player's state each frame, run networking, and later spawn/drive actors. Options were IPS/pchtxt patches (raw assembly, very limited), Skyline, or exlaunch.

## Decision
Use exlaunch, vendored into `client/` (upstream commit `f9f4b0dd`, 2026-08-26). It loads as `subsdk9`, supports trampoline/replace/inline hooks, dynamically links against the game's SDK (`nn::socket`, `nn::os`, ...), and is actively maintained. It runs the same way on Ryujinx and on Atmosphère.

## Consequences
- We write normal C++ instead of assembly patches.
- Building needs devkitPro/devkitA64 (CI handles this with the `devkitpro/devkita64` image).
- exlaunch is GPLv2, which fixes our license (see 0005).
- Updating exlaunch means re-copying upstream files outside `source/program/`; note the new commit in `client/EXLAUNCH.md` and the changelog.
