# 0004. Network on its own thread; read-only inline hooks

- **Date:** 2026-10-04
- **Status:** Accepted

## Context
BotW runs close to its frame budget on Switch. Network calls can stall, and a badly written hook can crash the game or corrupt its state.

## Decision
- All socket work runs on a dedicated thread (`net/client.cpp`, core 2, 20 Hz). The game thread only copies values in/out under a tiny spinlock, so a slow network can't drop frames.
- The first game hook is an **inline hook** that only reads registers (`x0` = player object) and returns, leaving the game's code path untouched. A wrong offset can produce wrong numbers but not a corrupted call.
- Until offsets are found, the module sends a generated test pattern so the network path can be tested on its own.

## Consequences
- Data the game thread sees from the network is up to ~50 ms old; smoothing (interpolation/extrapolation using `vel` and `game_tick`) will happen when remote players are drawn.
- Anything that has to *change* game behavior (spawning actors) will need trampoline hooks and more care.
