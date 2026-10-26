# Roadmap

Milestones are tracked as GitHub issues/milestones; this file is the overview. Update it when a milestone lands or plans change.

## M1: Network layer ✅ (code complete, awaiting in-game test)
- [x] Wire protocol (`protocol/`)
- [x] Relay server with join/leave/timeout, tests
- [x] Fake client for testing
- [x] Switch module: socket setup, join, 20 Hz state stream, test pattern
- [ ] Confirm the module loads in Ryujinx and joins the server

## M2: Real player data
- [x] Detect the game build at startup and only touch game memory on 1.0.0 ([decision 0009](decisions/0009-target-botw-1-0-0.md))
- [ ] Find the 1.0.0 offsets: the 1.6.0 research doesn't carry over ([what's needed](research/offsets-log.md))
- [ ] Module-side scan that finds `PlayerInfo::sInstance` on 1.0.0 by itself (look through main's data for a pointer whose `+0x60` actor has a sane matrix at `+0x398`, log candidates), so no Ghidra is needed
- [ ] Find per-frame player function and position/rotation fields ([guide](finding-offsets.md))
- [ ] Module sends real position
- [x] Read server address/name from a config file instead of compiling them in ([configuration](configuration.md); awaiting in-game test)

## M3: See each other
- [ ] Spawn a stand-in actor for each remote player
- [ ] Drive it from network state with interpolation
- [ ] Handle area/map changes and loading screens

## M4: Look right
- [ ] Sync animation state, equipment
- [ ] Gliding/climbing/swimming/riding

## Later ideas
Shared enemies, item drops, combat damage, shared world state (shrines, towers), in-game player list.
