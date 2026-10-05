# Roadmap

Milestones are tracked as GitHub issues/milestones; this file is the overview. Update it when a milestone lands or plans change.

## M1: Network layer ✅ (code complete, awaiting in-game test)
- [x] Wire protocol (`protocol/`)
- [x] Relay server with join/leave/timeout, tests
- [x] Fake client for testing
- [x] Switch module: socket setup, join, 20 Hz state stream, test pattern
- [ ] Confirm the module loads in Ryujinx and joins the server

## M2: Real player data
- [x] Find position/rotation: `PlayerInfo` pointer chain from the decomp, no per-frame function needed ([research](research/decomp-mapping.md), [decision 0007](decisions/0007-read-player-via-playerinfo.md)); **awaiting in-game verification**
- [x] Module sends real position (awaiting in-game test)
- [x] Read server address/name from a config file instead of compiling them in ([configuration](configuration.md); awaiting in-game test)

## M3: See each other
- [ ] Spawn a stand-in actor for each remote player
- [ ] Drive it from network state with interpolation
- [ ] Handle area/map changes and loading screens

## M4: Look right
- [ ] Sync animation state, equipment
- [ ] Gliding/climbing/swimming/riding

## Later ideas
Shared enemies, item drops, combat damage, shared world state (shrines, towers), in-game player list, version detection and refusing to run on non-1.6.0.
