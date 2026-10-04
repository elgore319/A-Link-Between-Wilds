"""Tests for the relay server. Run with:  python3 -m unittest -v  (from server/)"""
import asyncio
import unittest

import protocol as p
from albw_server import RelayServer, seq_newer

A = ("10.0.0.1", 5000)
B = ("10.0.0.2", 5000)
C = ("10.0.0.3", 5000)


class FakeClock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


class Harness:
    """Drives RelayServer without sockets, capturing what it sends."""

    def __init__(self, **kw):
        self.clock = FakeClock()
        self.server = RelayServer(clock=self.clock, **kw)
        self.sent: list[tuple[bytes, tuple]] = []
        self.server.send = lambda data, addr: self.sent.append((data, addr))

    def recv(self, data, addr):
        self.server.datagram_received(data, addr)

    def take(self, addr=None):
        out = [(d, a) for d, a in self.sent if addr is None or a == addr]
        self.sent = [(d, a) for d, a in self.sent if not (addr is None or a == addr)]
        return out

    def types_to(self, addr):
        return [p.parse_header(d).type for d, _ in self.take(addr)]

    def join(self, name, addr):
        self.recv(p.hello(name), addr)
        welcome = [d for d, a in self.sent if a == addr and p.parse_header(d).type == p.WELCOME][-1]
        return p.parse_header(welcome).player_id


class ProtocolTests(unittest.TestCase):
    def test_roundtrip_state(self):
        st = p.PlayerState(pos=(1.0, 2.0, 3.0), rot_y=0.5, vel=(0.0, -1.0, 0.0),
                           anim_id=7, flags=p.STATE_GLIDING, game_tick=99)
        pkt = p.player_state(2, st, seq=10)
        hdr = p.parse_header(pkt)
        self.assertEqual((hdr.type, hdr.player_id, hdr.seq), (p.PLAYER_STATE, 2, 10))
        self.assertEqual(p.PlayerState.unpack_body(pkt), st)

    def test_rejects_garbage(self):
        for bad in [b"", b"hello world!", p.hello("x")[:-1], b"\0" * 12]:
            with self.assertRaises(p.ProtocolError):
                p.parse_header(bad)

    def test_name_truncation(self):
        raw = p.encode_name("A" * 40)
        self.assertEqual(len(raw), p.NAME_LEN)
        self.assertEqual(p.decode_name(raw), "A" * 16)

    def test_seq_wraparound(self):
        self.assertTrue(seq_newer(1, 0))
        self.assertFalse(seq_newer(0, 1))
        self.assertTrue(seq_newer(0, 0xFFFFFFFF))
        self.assertFalse(seq_newer(5, 5))


class ServerTests(unittest.TestCase):
    def test_join_assigns_ids_and_announces(self):
        h = Harness()
        a = h.join("Link", A)
        self.assertEqual(a, 0)
        self.assertEqual(h.types_to(A), [p.WELCOME])
        b = h.join("Linkle", B)
        self.assertEqual(b, 1)
        # B learns about A; A learns about B.
        self.assertEqual(h.types_to(B), [p.WELCOME, p.PLAYER_JOIN])
        self.assertEqual(h.types_to(A), [p.PLAYER_JOIN])

    def test_duplicate_hello_keeps_same_id(self):
        h = Harness()
        a1 = h.join("Link", A)
        h.take()
        a2 = h.join("Link", A)
        self.assertEqual(a1, a2)
        self.assertEqual(len(h.server.players), 1)

    def test_full_server_rejects_fifth(self):
        h = Harness()
        for i in range(p.MAX_PLAYERS):
            h.join(f"P{i}", (f"10.0.1.{i}", 1))
        h.take()
        fifth = ("10.0.9.9", 1)
        h.recv(p.hello("Extra"), fifth)
        (data, _), = h.take(fifth)
        self.assertEqual(p.parse_header(data).type, p.REJECT)
        self.assertEqual(p.REJECT_BODY.unpack_from(data, p.HEADER.size)[0], p.REJECT_FULL)

    def test_bad_version_rejected(self):
        h = Harness()
        pkt = bytearray(p.hello("Old"))
        pkt[4] = 99  # version byte
        h.recv(bytes(pkt), A)
        (data, _), = h.take(A)
        self.assertEqual(p.REJECT_BODY.unpack_from(data, p.HEADER.size)[0], p.REJECT_BAD_VERSION)
        self.assertEqual(h.server.players, {})

    def test_state_relayed_to_others_only(self):
        h = Harness()
        a, b = h.join("Link", A), h.join("Linkle", B)
        h.join("Zelda", C)
        h.take()
        pkt = p.player_state(a, p.PlayerState(pos=(5, 6, 7)), seq=1)
        h.recv(pkt, A)
        self.assertEqual(h.take(A), [])
        self.assertEqual(h.take(B), [(pkt, B)])
        self.assertEqual(h.take(C), [(pkt, C)])

    def test_stale_state_dropped(self):
        h = Harness()
        a, _ = h.join("Link", A), h.join("Linkle", B)
        h.take()
        h.recv(p.player_state(a, p.PlayerState(pos=(2, 0, 0)), seq=5), A)
        h.recv(p.player_state(a, p.PlayerState(pos=(1, 0, 0)), seq=4), A)  # arrives late
        self.assertEqual(len(h.take(B)), 1)
        self.assertEqual(h.server.players[a].state.pos, (2.0, 0.0, 0.0))

    def test_spoofed_sender_ignored(self):
        h = Harness()
        a, b = h.join("Link", A), h.join("Linkle", B)
        h.take()
        # B pretends to be A.
        h.recv(p.player_state(a, p.PlayerState(pos=(9, 9, 9)), seq=1), B)
        self.assertEqual(h.take(), [])

    def test_ping_pong(self):
        h = Harness()
        a = h.join("Link", A)
        h.take()
        h.recv(p.ping(a, 123456), A)
        (data, _), = h.take(A)
        self.assertEqual(p.parse_header(data).type, p.PONG)
        self.assertEqual(p.PING_BODY.unpack_from(data, p.HEADER.size)[0], 123456)

    def test_bye_and_timeout(self):
        h = Harness(timeout=5.0)
        a, b = h.join("Link", A), h.join("Linkle", B)
        c = h.join("Zelda", C)
        h.take()
        h.recv(p.bye(a), A)
        self.assertNotIn(a, h.server.players)
        self.assertEqual(h.types_to(B), [p.PLAYER_LEAVE])
        h.take()
        # B keeps talking, C goes silent.
        h.clock.t = 4.0
        h.recv(p.ping(b, 0), B)
        h.clock.t = 6.0
        h.server.reap()
        self.assertEqual(set(h.server.players), {b})
        leave = [d for d, a_ in h.take(B) if p.parse_header(d).type == p.PLAYER_LEAVE]
        self.assertEqual(p.parse_header(leave[0]).player_id, c)
        # Freed slot gets reused.
        self.assertEqual(h.join("New", A), 0)


class SocketTest(unittest.TestCase):
    """End-to-end over real localhost UDP sockets."""

    def test_two_clients_over_udp(self):
        async def run():
            loop = asyncio.get_running_loop()
            server = RelayServer()
            st, _ = await loop.create_datagram_endpoint(lambda: server, local_addr=("127.0.0.1", 0))
            port = st.get_extra_info("sockname")[1]

            class Client(asyncio.DatagramProtocol):
                def __init__(self):
                    self.q = asyncio.Queue()

                def datagram_received(self, data, addr):
                    self.q.put_nowait(data)

            ta, ca = await loop.create_datagram_endpoint(Client, remote_addr=("127.0.0.1", port))
            tb, cb = await loop.create_datagram_endpoint(Client, remote_addr=("127.0.0.1", port))
            ta.sendto(p.hello("Link"))
            a_id = p.parse_header(await asyncio.wait_for(ca.q.get(), 2)).player_id
            tb.sendto(p.hello("Linkle"))
            await asyncio.wait_for(cb.q.get(), 2)  # WELCOME
            await asyncio.wait_for(cb.q.get(), 2)  # JOIN (Link)
            await asyncio.wait_for(ca.q.get(), 2)  # JOIN (Linkle)
            ta.sendto(p.player_state(a_id, p.PlayerState(pos=(10.0, 20.0, 30.0)), seq=1))
            got = await asyncio.wait_for(cb.q.get(), 2)
            for t in (ta, tb, st):
                t.close()
            return p.PlayerState.unpack_body(got).pos

        self.assertEqual(asyncio.run(run()), (10.0, 20.0, 30.0))


if __name__ == "__main__":
    unittest.main()
