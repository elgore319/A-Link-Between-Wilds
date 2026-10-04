# Wire protocol (v1)

Source of truth: [`protocol/albw_protocol.h`](../protocol/albw_protocol.h), mirrored in [`server/protocol.py`](../server/protocol.py). Packet sizes are checked by `static_assert`s in the header and by CI.

**Changing a packet:** update both files, bump `ALBW_PROTOCOL_VERSION`, update the table below, and note it in the changelog. Old clients will be rejected with `REJECT_BAD_VERSION` instead of misreading data.

## Basics
- UDP, one packet per datagram, little-endian, no padding. Default port **55420**.
- Every packet starts with a 12-byte header: `magic "ALBW"`, `version`, `type`, `player_id`, `seq`.
- `seq` increases per sender. Receivers drop packets older than the newest they've seen (wraparound-safe).
- Max 4 players, ids 0–3. `0xFFFF` = no id yet.

## Packets

| Type | # | Dir | Size | Body |
|---|---|---|---|---|
| HELLO | 1 | C→S | 28 | name[16] |
| WELCOME | 2 | S→C | 14 | max_players u8, player_count u8. Your id is in `hdr.player_id` |
| REJECT | 3 | S→C | 13 | reason u8 (1 = full, 2 = bad version) |
| PLAYER_STATE | 4 | C→S→C | 52 | pos f32×3, rot_y f32, vel f32×3, anim_id u32, flags u32, game_tick u32 |
| PLAYER_JOIN | 5 | S→C | 28 | name[16]; `hdr.player_id` = who joined |
| PLAYER_LEAVE | 6 | S→C | 12 | none; `hdr.player_id` = who left |
| PING | 7 | C→S | 20 | client_time u64 |
| PONG | 8 | S→C | 20 | client_time echoed |
| BYE | 9 | C→S | 12 | none |

## Session flow
1. Client sends HELLO every ~1 s until it gets WELCOME or REJECT. A repeated HELLO from the same address gets the same id back.
2. Server sends the newcomer a PLAYER_JOIN for each existing player, and everyone else a PLAYER_JOIN for the newcomer.
3. Client streams PLAYER_STATE at 20 Hz; the server forwards each one unchanged to all other players.
4. Client sends PING once a second. Players silent for 5 s are dropped (PLAYER_LEAVE to everyone else).
5. BYE leaves immediately.

The server only accepts non-HELLO packets from the address that joined with that `player_id`, so one client can't impersonate another.
