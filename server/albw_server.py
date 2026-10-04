#!/usr/bin/env python3
"""A Link Between Wilds - relay server.

A small UDP relay: clients send HELLO to join, then stream PLAYER_STATE packets,
which the server forwards to every other connected player. Players that go quiet
for longer than the timeout are dropped and everyone else is told.

The server is deliberately "dumb" for now (it relays, it doesn't simulate), which
is all the first milestone needs. Run it on your PC or home server:

    python3 albw_server.py --port 55420
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import time
from dataclasses import dataclass, field

import protocol as p

log = logging.getLogger("albw")

Addr = tuple[str, int]


@dataclass
class Player:
    player_id: int
    name: str
    addr: Addr
    last_seen: float
    last_state_seq: int = -1
    seq_out: int = 0
    state: p.PlayerState = field(default_factory=p.PlayerState)

    def next_seq(self) -> int:
        self.seq_out = (self.seq_out + 1) & 0xFFFFFFFF
        return self.seq_out


def seq_newer(a: int, b: int) -> bool:
    """True if sequence a is newer than b, handling 32-bit wraparound."""
    return ((a - b) & 0xFFFFFFFF) < 0x80000000 and a != b


class RelayServer(asyncio.DatagramProtocol):
    def __init__(self, timeout: float = 5.0, clock=time.monotonic):
        self.timeout = timeout
        self.clock = clock
        self.players: dict[int, Player] = {}
        self.by_addr: dict[Addr, int] = {}
        self.transport: asyncio.DatagramTransport | None = None

    # --- asyncio plumbing ---------------------------------------------------

    def connection_made(self, transport):
        self.transport = transport

    def send(self, data: bytes, addr: Addr) -> None:
        if self.transport is not None:
            self.transport.sendto(data, addr)

    def datagram_received(self, data: bytes, addr: Addr) -> None:
        try:
            hdr = p.parse_header(data)
        except p.ProtocolError as e:
            log.debug("dropped packet from %s: %s", addr, e)
            return

        if hdr.type == p.HELLO:
            self.on_hello(hdr, data, addr)
            return

        # Every other packet must come from a joined player, from their own address.
        pid = self.by_addr.get(addr)
        if pid is None or pid != hdr.player_id or hdr.version != p.PROTOCOL_VERSION:
            return
        player = self.players[pid]
        player.last_seen = self.clock()

        if hdr.type == p.PLAYER_STATE:
            self.on_state(player, hdr, data)
        elif hdr.type == p.PING:
            (client_time,) = p.PING_BODY.unpack_from(data, p.HEADER.size)
            self.send(p.pong(pid, client_time, player.next_seq()), addr)
        elif hdr.type == p.BYE:
            self.remove_player(pid, "left")

    # --- handlers -----------------------------------------------------------

    def on_hello(self, hdr: p.Header, data: bytes, addr: Addr) -> None:
        if hdr.version != p.PROTOCOL_VERSION:
            log.info("rejecting %s: protocol v%d, server is v%d", addr, hdr.version, p.PROTOCOL_VERSION)
            self.send(p.reject(p.REJECT_BAD_VERSION), addr)
            return

        (raw_name,) = p.HELLO_BODY.unpack_from(data, p.HEADER.size)
        name = p.decode_name(raw_name) or "Link"

        # Re-sent HELLO (lost WELCOME): answer again with the same id.
        if addr in self.by_addr:
            player = self.players[self.by_addr[addr]]
            player.last_seen = self.clock()
            self.send(p.welcome(player.player_id, len(self.players), player.next_seq()), addr)
            return

        free = [i for i in range(p.MAX_PLAYERS) if i not in self.players]
        if not free:
            log.info("rejecting %s (%s): server full", addr, name)
            self.send(p.reject(p.REJECT_FULL), addr)
            return

        pid = free[0]
        player = Player(pid, name, addr, self.clock())
        self.players[pid] = player
        self.by_addr[addr] = pid
        log.info("player %d '%s' joined from %s:%d (%d/%d)",
                 pid, name, addr[0], addr[1], len(self.players), p.MAX_PLAYERS)

        self.send(p.welcome(pid, len(self.players), player.next_seq()), addr)
        # Tell the newcomer about everyone already here, and everyone about the newcomer.
        for other in self.players.values():
            if other.player_id == pid:
                continue
            self.send(p.player_join(other.player_id, other.name, player.next_seq()), addr)
            self.send(p.player_join(pid, name, other.next_seq()), other.addr)

    def on_state(self, player: Player, hdr: p.Header, data: bytes) -> None:
        # UDP can reorder; ignore anything older than what we've already relayed.
        if player.last_state_seq >= 0 and not seq_newer(hdr.seq, player.last_state_seq):
            return
        player.last_state_seq = hdr.seq
        player.state = p.PlayerState.unpack_body(data)
        # Forward verbatim: the header already carries the sender's id and seq.
        for other in self.players.values():
            if other.player_id != player.player_id:
                self.send(data, other.addr)

    def remove_player(self, pid: int, reason: str) -> None:
        player = self.players.pop(pid, None)
        if player is None:
            return
        self.by_addr.pop(player.addr, None)
        log.info("player %d '%s' %s (%d/%d)", pid, player.name, reason, len(self.players), p.MAX_PLAYERS)
        for other in self.players.values():
            self.send(p.player_leave(pid, other.next_seq()), other.addr)

    def reap(self) -> None:
        now = self.clock()
        for pid in [pid for pid, pl in self.players.items() if now - pl.last_seen > self.timeout]:
            self.remove_player(pid, "timed out")


async def main() -> None:
    ap = argparse.ArgumentParser(description="A Link Between Wilds relay server")
    ap.add_argument("--host", default="0.0.0.0", help="address to listen on (default: all interfaces)")
    ap.add_argument("--port", type=int, default=p.DEFAULT_PORT)
    ap.add_argument("--timeout", type=float, default=5.0, help="seconds before a silent player is dropped")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s", datefmt="%H:%M:%S")

    loop = asyncio.get_running_loop()
    server = RelayServer(timeout=args.timeout)
    transport, _ = await loop.create_datagram_endpoint(lambda: server, local_addr=(args.host, args.port))
    log.info("A Link Between Wilds server listening on %s:%d (max %d players)",
             args.host, args.port, p.MAX_PLAYERS)
    try:
        while True:
            await asyncio.sleep(1.0)
            server.reap()
    finally:
        transport.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
