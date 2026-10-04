"""Python mirror of protocol/albw_protocol.h. Keep the two in sync."""
from __future__ import annotations

import struct
from dataclasses import dataclass

MAGIC = 0x57424C41  # "ALBW"
PROTOCOL_VERSION = 1
DEFAULT_PORT = 55420
MAX_PLAYERS = 4
NAME_LEN = 16
INVALID_PLAYER = 0xFFFF

# Packet types
HELLO = 1
WELCOME = 2
REJECT = 3
PLAYER_STATE = 4
PLAYER_JOIN = 5
PLAYER_LEAVE = 6
PING = 7
PONG = 8
BYE = 9

# Reject reasons
REJECT_FULL = 1
REJECT_BAD_VERSION = 2

# State flags
STATE_GROUNDED = 1 << 0
STATE_GLIDING = 1 << 1
STATE_SWIMMING = 1 << 2
STATE_CLIMBING = 1 << 3
STATE_IN_MENU = 1 << 4

HEADER = struct.Struct("<IBBHI")                 # 12
HELLO_BODY = struct.Struct(f"<{NAME_LEN}s")       # 16 -> 28
WELCOME_BODY = struct.Struct("<BB")               # 2  -> 14
REJECT_BODY = struct.Struct("<B")                 # 1  -> 13
STATE_BODY = struct.Struct("<3ff3fIII")           # 40 -> 52
JOIN_BODY = struct.Struct(f"<{NAME_LEN}s")        # 16 -> 28
PING_BODY = struct.Struct("<Q")                   # 8  -> 20

# Full packet sizes, mirrored by static_asserts in albw_protocol.h.
SIZES = {
    HELLO: HEADER.size + HELLO_BODY.size,
    WELCOME: HEADER.size + WELCOME_BODY.size,
    REJECT: HEADER.size + REJECT_BODY.size,
    PLAYER_STATE: HEADER.size + STATE_BODY.size,
    PLAYER_JOIN: HEADER.size + JOIN_BODY.size,
    PLAYER_LEAVE: HEADER.size,
    PING: HEADER.size + PING_BODY.size,
    PONG: HEADER.size + PING_BODY.size,
    BYE: HEADER.size,
}


@dataclass
class Header:
    type: int
    player_id: int = INVALID_PLAYER
    seq: int = 0
    magic: int = MAGIC
    version: int = PROTOCOL_VERSION

    def pack(self) -> bytes:
        return HEADER.pack(self.magic, self.version, self.type, self.player_id, self.seq)


@dataclass
class PlayerState:
    pos: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rot_y: float = 0.0
    vel: tuple[float, float, float] = (0.0, 0.0, 0.0)
    anim_id: int = 0
    flags: int = 0
    game_tick: int = 0

    def pack_body(self) -> bytes:
        return STATE_BODY.pack(*self.pos, self.rot_y, *self.vel,
                               self.anim_id, self.flags, self.game_tick)

    @classmethod
    def unpack_body(cls, data: bytes) -> "PlayerState":
        v = STATE_BODY.unpack_from(data, HEADER.size)
        return cls(pos=v[0:3], rot_y=v[3], vel=v[4:7],
                   anim_id=v[7], flags=v[8], game_tick=v[9])


class ProtocolError(ValueError):
    pass


def parse_header(data: bytes) -> Header:
    """Validate and decode the header. Raises ProtocolError on garbage."""
    if len(data) < HEADER.size:
        raise ProtocolError("packet shorter than header")
    magic, version, ptype, player_id, seq = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise ProtocolError("bad magic")
    expected = SIZES.get(ptype)
    if expected is None:
        raise ProtocolError(f"unknown packet type {ptype}")
    # Version is checked by the server on HELLO so it can send a REJECT;
    # size only matters when the version matches.
    if version == PROTOCOL_VERSION and len(data) != expected:
        raise ProtocolError(f"type {ptype}: expected {expected} bytes, got {len(data)}")
    return Header(type=ptype, player_id=player_id, seq=seq, magic=magic, version=version)


def encode_name(name: str) -> bytes:
    return name.encode("utf-8")[:NAME_LEN].ljust(NAME_LEN, b"\0")


def decode_name(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("utf-8", errors="replace")


# --- builders -------------------------------------------------------------

def hello(name: str, seq: int = 0) -> bytes:
    return Header(HELLO, INVALID_PLAYER, seq).pack() + HELLO_BODY.pack(encode_name(name))


def welcome(player_id: int, player_count: int, seq: int = 0) -> bytes:
    return Header(WELCOME, player_id, seq).pack() + WELCOME_BODY.pack(MAX_PLAYERS, player_count)


def reject(reason: int, seq: int = 0) -> bytes:
    return Header(REJECT, INVALID_PLAYER, seq).pack() + REJECT_BODY.pack(reason)


def player_state(player_id: int, state: PlayerState, seq: int) -> bytes:
    return Header(PLAYER_STATE, player_id, seq).pack() + state.pack_body()


def player_join(player_id: int, name: str, seq: int = 0) -> bytes:
    return Header(PLAYER_JOIN, player_id, seq).pack() + JOIN_BODY.pack(encode_name(name))


def player_leave(player_id: int, seq: int = 0) -> bytes:
    return Header(PLAYER_LEAVE, player_id, seq).pack()


def ping(player_id: int, client_time: int, seq: int = 0) -> bytes:
    return Header(PING, player_id, seq).pack() + PING_BODY.pack(client_time)


def pong(player_id: int, client_time: int, seq: int = 0) -> bytes:
    return Header(PONG, player_id, seq).pack() + PING_BODY.pack(client_time)


def bye(player_id: int, seq: int = 0) -> bytes:
    return Header(BYE, player_id, seq).pack()
