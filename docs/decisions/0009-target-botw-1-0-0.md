# 0009. Target BotW 1.0.0 (no update) instead of 1.6.0

- **Date:** 2026-10-26
- **Status:** Accepted (supersedes [0002](0002-target-botw-1-6-0.md))

## Context
[0002](0002-target-botw-1-6-0.md) picked 1.6.0, the final update, because most mods and community research use it. But the copy Lee has dumped is the base game, 1.0.0, and getting the 1.6.0 update means dumping it separately from a Switch. Lee chose to build for 1.0.0 instead.

Every offset is tied to one exact build, so this is a real trade-off, not a rename:
- The 1.6.0 research (the `PlayerInfo` global at `main+0x2CA1140`, from Pistonight/botw-symbols and botw-save-state) does not apply to 1.0.0. No public 1.0.0 symbol listing or 1.0.0 mod was found to take addresses from (see the [offsets log](../research/offsets-log.md)).
- The decomp targets 1.5.0. Its *method* still works, and struct layouts may well be the same, but 1.0.0 predates the DLC (1.3.0 and 1.4.0 added AI code and subsystems), so every field offset needs confirming on 1.0.0.
- 1.0.0 is built against an older Nintendo SDK, so the SDK functions the module imports (`nn::socket`, `nn::fs::MountSdCardForDebug`) are unconfirmed on it, just as they still were on 1.6.0.

## Decision
- Support only BotW Switch **1.0.0 with no update**. Program ID `01007EF00011E000` (the same for every version). Main executable build ID `2094F152836683A3632C6153FD84AB80` ([zeldamods.org Versions](https://zeldamods.org/wiki/Versions)).
- The module reads the main executable's GNU build ID at startup (`client/source/program/game/game_version.cpp`). It only hooks or reads the game when that ID is 1.0.0's. On any other build it logs why, still runs the network layer and sends the test pattern. This also covers the old 0002 "refuse to run on a mismatch rather than crash" follow-up.
- `game_offsets.hpp` holds 1.0.0 values. 1.5.0/1.6.0 values stay in `docs/research/` as references for porting, not as values to use.

## Consequences
- Every player needs 1.0.0. Friends who have the update installed must switch it off in Ryujinx (right-click BotW > *Manage Title Updates* > *No Update*). The module and `test-m1` say so if they detect another version.
- M2 can't reuse the 1.6.0 pointer chain directly: `PlayerInfo::sInstance` has to be found on 1.0.0 (Ghidra on Lee's own dump, or a planned module-side scan), and the `+0x60`/`+0x398` field offsets confirmed.
- No DLC content on 1.0.0. Nothing in the roadmap needs it.
- If we ever want 1.6.0 too, exlaunch's versioned tables (`offsets.hpp`, `version.hpp`) can hold one offset set per build; `game_version.cpp` already tells them apart.
