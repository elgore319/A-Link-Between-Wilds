# 0007. Read the player from the PlayerInfo pointer chain on the network thread

- **Date:** 2026-10-05
- **Status:** Accepted (pending in-game verification of the offsets)
- **Partly supersedes:** [0004](0004-network-thread-and-inline-hooks.md), only its plan to get the player's position from an inline hook on a per-frame player function. 0004's network-thread design stands.

## Context
0004 planned to hook a once-per-frame player function and read the position from the object in `x0`, which meant first finding that function in 1.6.0. The research in [decomp-mapping.md](../research/decomp-mapping.md) found a global path to the same data that doesn't need a function at all: `[main+0x2CA1140]` (`PlayerInfo::sInstance`) → `+0x60` (`mPlayerActor`) → `+0x398` (`Actor::mMtx`). It comes from the 1.5.0 decomp layout, a 1.6.0 symbol listing, and a working 1.6.0 mod that uses the same chain.

## Decision
- The network thread follows that chain once per send tick (20 Hz) and sends the translation (X/Y/Z) and the yaw from the rotation part (`game/player_reader.cpp`).
- **Every pointer is checked with `svcQueryMemory`** (mapped, readable, aligned) before it's read, so a bad offset or a freed actor can't fault on unmapped memory.
- **Every result is sanity-checked** (`game/player_math.hpp`, host-tested): finite, within ±20000 of the origin, not exactly the origin, unit-length rotation rows. This rejects garbage from a stale pointer into reused memory.
- When there's no valid player (title screen, loading), the module **stops sending state** and keeps pinging, so it stays joined. The first sample after a gap resets the velocity base so it doesn't report a huge jump.
- Every 5 s it logs the position it read, and `PlayerInfo::mPlayerPos` alongside it, so the offsets can be checked from the Ryujinx log without Cheat Engine.
- `ReadPlayerFromMemory = false` in `albw_config.hpp` goes back to the test pattern. The hook path is kept as a second option in `main.cpp`.

## Consequences
- M2 no longer depends on finding the player's update function.
- Reads aren't synchronized with the game thread. One sample can mix rows from two frames (sub-frame error, invisible at 20 Hz), and the actor can be freed between checking and reading. The memory-map check stops crashes; the sanity checks stop most garbage. If garbage still gets through in testing, move the read onto the game thread with a hook on any cheap per-frame function. It wouldn't need to be the player's.
- Yaw's sign and zero direction are unconfirmed and need checking in-game. A wrong convention only rotates remote players' facing; it doesn't affect position.
- These offsets are 1.6.0-only, consistent with [0002](0002-target-botw-1-6-0.md).
