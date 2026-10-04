#!/usr/bin/env python3
"""Fake player for testing the server (and later, the Switch module) without the game.

It joins, runs in a circle, and prints whatever other players send. Start two of
these against one server to watch them see each other, or one of these alongside
the real game to give your Link someone to look at once player rendering works.

    python3 tools/fake_client.py --server 127.0.0.1 --name Linkle
"""
from __future__ import annotations

import argparse
import math
import os
import socket
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "server"))
import protocol as p  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--server", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=p.DEFAULT_PORT)
    ap.add_argument("--name", default="FakeLink")
    ap.add_argument("--rate", type=float, default=20.0, help="state packets per second")
    ap.add_argument("--radius", type=float, default=5.0)
    ap.add_argument("--center", type=float, nargs=3, default=(0.0, 0.0, 0.0), metavar=("X", "Y", "Z"),
                    help="world position to circle around (e.g. near your own Link)")
    ap.add_argument("--quiet", action="store_true", help="don't print other players' states")
    args = ap.parse_args()

    server = (args.server, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)

    # Join, retrying since UDP can drop the HELLO or WELCOME.
    my_id = None
    for attempt in range(5):
        sock.sendto(p.hello(args.name), server)
        try:
            data, _ = sock.recvfrom(2048)
        except socket.timeout:
            print(f"no answer from {server[0]}:{server[1]}, retrying ({attempt + 1}/5)...")
            continue
        hdr = p.parse_header(data)
        if hdr.type == p.REJECT:
            reason = p.REJECT_BODY.unpack_from(data, p.HEADER.size)[0]
            sys.exit(f"rejected: {'server full' if reason == p.REJECT_FULL else 'protocol version mismatch'}")
        if hdr.type == p.WELCOME:
            my_id = hdr.player_id
            break
    if my_id is None:
        sys.exit("couldn't reach the server")

    print(f"joined as player {my_id} ('{args.name}'). Ctrl+C to leave.")
    sock.setblocking(False)

    names: dict[int, str] = {}
    seq = 0
    tick = 0
    period = 1.0 / args.rate
    last_ping = 0.0
    start = time.monotonic()
    try:
        while True:
            now = time.monotonic()
            t = now - start
            angle = t * 0.8
            cx, cy, cz = args.center
            pos = (cx + math.cos(angle) * args.radius, cy, cz + math.sin(angle) * args.radius)
            vel = (-math.sin(angle) * args.radius * 0.8, 0.0, math.cos(angle) * args.radius * 0.8)
            state = p.PlayerState(pos=pos, rot_y=angle + math.pi / 2, vel=vel,
                                  flags=p.STATE_GROUNDED, game_tick=tick)
            seq += 1
            tick += 1
            sock.sendto(p.player_state(my_id, state, seq), server)

            if now - last_ping > 1.0:
                sock.sendto(p.ping(my_id, int(now * 1e6)), server)
                last_ping = now

            while True:
                try:
                    data, _ = sock.recvfrom(2048)
                except (BlockingIOError, InterruptedError):
                    break
                try:
                    hdr = p.parse_header(data)
                except p.ProtocolError:
                    continue
                if hdr.type == p.PLAYER_JOIN:
                    (raw,) = p.JOIN_BODY.unpack_from(data, p.HEADER.size)
                    names[hdr.player_id] = p.decode_name(raw)
                    print(f"+ player {hdr.player_id} '{names[hdr.player_id]}' joined")
                elif hdr.type == p.PLAYER_LEAVE:
                    print(f"- player {hdr.player_id} '{names.pop(hdr.player_id, '?')}' left")
                elif hdr.type == p.PLAYER_STATE and not args.quiet:
                    s = p.PlayerState.unpack_body(data)
                    who = names.get(hdr.player_id, str(hdr.player_id))
                    print(f"  {who:>16}: pos=({s.pos[0]:8.2f}, {s.pos[1]:8.2f}, {s.pos[2]:8.2f}) "
                          f"rot={s.rot_y:5.2f} seq={hdr.seq}", end="\r")
                elif hdr.type == p.PONG:
                    (sent,) = p.PING_BODY.unpack_from(data, p.HEADER.size)
                    rtt_ms = (time.monotonic() * 1e6 - sent) / 1000
                    if args.quiet:
                        print(f"rtt {rtt_ms:.1f} ms", end="\r")

            time.sleep(max(0.0, period - (time.monotonic() - now)))
    except KeyboardInterrupt:
        sock.sendto(p.bye(my_id), server)
        print("\nleft the server.")


if __name__ == "__main__":
    main()
