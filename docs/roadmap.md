# Roadmap

Milestones are tracked as GitHub issues/milestones; this file is the overview. Update it when a milestone lands or plans change.

## M1: Network layer ✅ (code complete, awaiting in-game test)
- [x] Wire protocol (`protocol/`)
- [x] Relay server with join/leave/timeout, tests
- [x] Fake client for testing
- [x] Switch module: socket setup, join, 20 Hz state stream, test pattern
- [ ] Confirm the module loads in Ryujinx and joins the server

## M2: Real player data
- [ ] Find per-frame player function and position/rotation fields ([guide](finding-offsets.md))
- [ ] Module sends real position
- [x] Read server address/name from a config file instead of compiling them in ([configuration](configuration.md); awaiting in-game test)

## Friend-ready setup ([decision 0008](decisions/0008-friend-proof-setup.md))
Runs alongside M3/M4; the first playable release can't ship without it.
- [ ] Decide how friends reach the host's server with no router setup (Tailscale / port forward / home-server relay): decision record
- [ ] Host launcher: one click starts the server and shows a join code
- [ ] Player installer (*ALBW Setup*): find/set up Ryujinx, check game 1.6.0 + keys + firmware with plain-English fixes, install/update the mod, ask name + join code, write `config.ini`
- [ ] Join code format (encodes host address/port), shared by launcher and installer
- [ ] Player-facing errors in the mod itself (e.g. on-screen "couldn't reach Lee's server" instead of only a log line)
- [ ] Test the full flow on a clean Windows account, following only the player instructions

## M3: See each other
- [ ] Spawn a stand-in actor for each remote player
- [ ] Drive it from network state with interpolation
- [ ] Handle area/map changes and loading screens

## M4: Look right
- [ ] Sync animation state, equipment
- [ ] Gliding/climbing/swimming/riding

## Later ideas
Shared enemies, item drops, combat damage, shared world state (shrines, towers), in-game player list, version detection and refusing to run on non-1.6.0.
