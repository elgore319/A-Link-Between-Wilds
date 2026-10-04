# 0003. Dumb UDP relay server in Python

- **Date:** 2026-10-04
- **Status:** Accepted

## Context
Players need to exchange position/animation many times per second. Options: peer-to-peer, an authoritative server that simulates the world, or a relay that just forwards packets.

## Decision
A relay server (`server/albw_server.py`) over UDP:
- **UDP, not TCP:** position updates are only useful while fresh. A lost packet should be skipped, not retransmitted and delivered late. Each packet carries a sequence number so stale ones are dropped.
- **Relay, not peer-to-peer:** avoids NAT punch-through and means each Switch sends one stream instead of one per player.
- **Not authoritative (yet):** the server does no game simulation. That's enough for "see each other"; syncing enemies/world state may need more later.
- **Python:** fast to iterate and test; 4 players at 20 Hz is trivial load.

## Consequences
- Anything that must arrive (e.g. "player picked up item") will need an ack/resend layer on top. Not needed for milestone 1.
- No cheating protection; fine among friends.
- If the server ever needs to simulate the world, it may be rewritten (C++ could share game structs with the client). This record would then be superseded.
