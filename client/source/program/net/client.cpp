#include "client.hpp"

#include <nn/os.hpp>
#include "nn_socket.hpp"
#include "program/albw_config.hpp"
#include "program/loggers.hpp"

namespace albw::net {

    namespace {
        /* nn::socket needs a page-aligned memory pool. These are the sizes commonly
           used by other Switch online mods; shrink if the game runs short on memory. */
        constexpr size_t SocketPoolSize      = 0x600000;
        constexpr size_t SocketAllocPoolSize = 0x20000;
        constexpr s32    SocketConcurrency   = 14;
        alignas(0x1000) u8 s_socket_pool[SocketPoolSize];

        constexpr size_t ThreadStackSize = 0x8000;
        alignas(0x1000) u8 s_thread_stack[ThreadStackSize];
        nn::os::ThreadType s_thread;

        Client s_client;

        void Sleep(s64 ms) { nn::os::SleepThread(nn::TimeSpan::FromMilliSeconds(ms)); }

        void CopyName(char* dst, const char* src, size_t max) {
            size_t i = 0;
            for (; i < max && src[i] != '\0'; i++) dst[i] = src[i];
            for (; i < max; i++) dst[i] = '\0';
        }
    }

    Client& GetClient() { return s_client; }

    void Client::Lock() { while (m_lock.test_and_set(std::memory_order_acquire)) { /* spin */ } }
    void Client::Unlock() { m_lock.clear(std::memory_order_release); }

    void Client::Start() {
        if (m_status.load() != Status::Off)
            return;
        m_status = Status::Connecting;

        /* Priority 16 is roughly "normal"; core 2 keeps us off the game's busiest core. */
        Result rc = nn::os::CreateThread(&s_thread, ThreadEntry, this, s_thread_stack, ThreadStackSize, 16, 2);
        if (rc != 0) {
            Logging.Log("[albw] CreateThread failed: 0x%x", rc);
            m_status = Status::Error;
            return;
        }
        nn::os::StartThread(&s_thread);
    }

    void Client::ThreadEntry(void* arg) {
        static_cast<Client*>(arg)->Run();
    }

    void Client::SetLocalState(const float pos[3], float rot_y, u32 game_tick) {
        Lock();
        m_pos[0] = pos[0]; m_pos[1] = pos[1]; m_pos[2] = pos[2];
        m_rot_y = rot_y;
        m_game_tick = game_tick;
        m_have_local = true;
        Unlock();
    }

    bool Client::GetRemote(int id, RemotePlayer& out) {
        if (id < 0 || id >= ALBW_MAX_PLAYERS)
            return false;
        Lock();
        out = m_remote[id];
        Unlock();
        return out.active;
    }

    bool Client::OpenSocket() {
        Result rc = nn::socket::Initialize(s_socket_pool, SocketPoolSize, SocketAllocPoolSize, SocketConcurrency);
        /* A non-zero result can mean the game already initialized sockets; carry on and
           let Socket() tell us whether networking actually works. */
        Logging.Log("[albw] nn::socket::Initialize -> 0x%x", rc);

        m_fd = nn::socket::Socket(AfInet, SockDgram, IpProtoUdp);
        if (m_fd < 0) {
            Logging.Log("[albw] Socket() failed, errno %d", nn::socket::GetLastErrno());
            return false;
        }

        sockaddr_in addr = {};
        addr.sin_len = sizeof(addr);
        addr.sin_family = AfInet;
        addr.sin_port = nn::socket::InetHtons(config::ServerPort);
        if (nn::socket::InetAton(config::ServerIp, &addr.sin_addr) == 0) {
            Logging.Log("[albw] bad server IP '%s'", config::ServerIp);
            return false;
        }

        /* "Connecting" a UDP socket just fixes the destination, so Send/Recv only
           talk to the server and packets from anyone else are dropped by the OS. */
        if (nn::socket::Connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            Logging.Log("[albw] Connect() failed, errno %d", nn::socket::GetLastErrno());
            return false;
        }
        Logging.Log("[albw] socket ready, server %s:%d", config::ServerIp, config::ServerPort);
        return true;
    }

    void Client::SendPacket(const void* data, size_t size) {
        nn::socket::Send(m_fd, data, size, 0);
    }

    void Client::Join() {
        albw_hello hello = {};
        CopyName(hello.name, config::PlayerName, ALBW_NAME_LEN);

        int attempts = 0;
        while (m_status.load() == Status::Connecting) {
            albw_init_header(&hello.hdr, ALBW_PKT_HELLO, ALBW_INVALID_PLAYER, ++m_seq);
            SendPacket(&hello, sizeof(hello));
            if (attempts++ % 10 == 0)
                Logging.Log("[albw] sent HELLO (attempt %d)", attempts);

            /* Wait up to ~1s for WELCOME/REJECT, then resend. */
            for (int i = 0; i < 20 && m_status.load() == Status::Connecting; i++) {
                Pump();
                Sleep(50);
            }
        }
    }

    void Client::Pump() {
        alignas(8) u8 buf[512];
        while (true) {
            s64 n = nn::socket::Recv(m_fd, buf, sizeof(buf), MsgDontWait);
            if (n <= 0)
                return;
            HandlePacket(buf, static_cast<size_t>(n));
        }
    }

    void Client::HandlePacket(const u8* data, size_t size) {
        if (size < sizeof(albw_header))
            return;
        albw_header hdr;
        __builtin_memcpy(&hdr, data, sizeof(hdr));
        if (hdr.magic != ALBW_MAGIC || hdr.version != ALBW_PROTOCOL_VERSION)
            return;

        switch (hdr.type) {
            case ALBW_PKT_WELCOME: {
                if (size != sizeof(albw_welcome)) return;
                m_my_id = hdr.player_id;
                m_status = Status::Connected;
                Logging.Log("[albw] joined as player %d", hdr.player_id);
                break;
            }
            case ALBW_PKT_REJECT: {
                if (size != sizeof(albw_reject)) return;
                albw_reject rej;
                __builtin_memcpy(&rej, data, sizeof(rej));
                Logging.Log("[albw] server rejected us (reason %d)", rej.reason);
                m_status = Status::Rejected;
                break;
            }
            case ALBW_PKT_PLAYER_JOIN: {
                if (size != sizeof(albw_player_join) || hdr.player_id >= ALBW_MAX_PLAYERS) return;
                albw_player_join join;
                __builtin_memcpy(&join, data, sizeof(join));
                Lock();
                RemotePlayer& rp = m_remote[hdr.player_id];
                rp = RemotePlayer {};
                rp.active = true;
                CopyName(rp.name, join.name, ALBW_NAME_LEN);
                rp.name[ALBW_NAME_LEN] = '\0';
                Unlock();
                Logging.Log("[albw] player %d joined", hdr.player_id);
                break;
            }
            case ALBW_PKT_PLAYER_LEAVE: {
                if (hdr.player_id >= ALBW_MAX_PLAYERS) return;
                Lock();
                m_remote[hdr.player_id].active = false;
                Unlock();
                Logging.Log("[albw] player %d left", hdr.player_id);
                break;
            }
            case ALBW_PKT_PLAYER_STATE: {
                if (size != sizeof(albw_player_state) || hdr.player_id >= ALBW_MAX_PLAYERS) return;
                Lock();
                RemotePlayer& rp = m_remote[hdr.player_id];
                /* Drop out-of-order packets (signed compare handles wraparound). */
                if (!rp.active || rp.packets == 0 || static_cast<s32>(hdr.seq - rp.last_seq) > 0) {
                    __builtin_memcpy(&rp.last, data, sizeof(rp.last));
                    rp.last_seq = hdr.seq;
                    rp.packets++;
                    rp.active = true;
                }
                Unlock();
                break;
            }
            default:
                break;
        }
    }

    void Client::SendState() {
        albw_player_state st = {};
        float dt = 1.0f / config::SendRateHz;

        Lock();
        bool have = m_have_local;
        for (int i = 0; i < 3; i++) {
            st.pos[i] = m_pos[i];
            st.vel[i] = (m_pos[i] - m_prev_pos[i]) / dt;
            m_prev_pos[i] = m_pos[i];
        }
        st.rot_y = m_rot_y;
        st.game_tick = m_game_tick;
        Unlock();

        if (!have)
            return;
        st.flags = ALBW_STATE_GROUNDED;
        albw_init_header(&st.hdr, ALBW_PKT_PLAYER_STATE, static_cast<u16>(m_my_id.load()), ++m_seq);
        SendPacket(&st, sizeof(st));
    }

    void Client::StepTestPattern() {
        /* Rotate a 5-unit radius vector a little each send (no libm needed). At 20 Hz this
           is one lap every ~8 seconds. cos/sin of 0.04 rad: */
        constexpr float c = 0.99920011f, s = 0.03998933f;
        float x = m_test_dir[0], z = m_test_dir[1];
        m_test_dir[0] = x * c - z * s;
        m_test_dir[1] = x * s + z * c;
        float pos[3] = { m_test_dir[0], 0.0f, m_test_dir[1] };
        SetLocalState(pos, 0.0f, ++m_test_tick);
    }

    void Client::Run() {
        /* Give the game a few seconds to finish booting before we touch the network. */
        Sleep(5000);

        if (!OpenSocket()) {
            m_status = Status::Error;
            return;
        }

        Join();
        if (m_status.load() != Status::Connected)
            return;

        const s64 period_ms = 1000 / config::SendRateHz;
        int ticks_since_ping = 0;
        while (true) {
            Pump();
            if (m_test_pattern)
                StepTestPattern();
            SendState();

            if (++ticks_since_ping >= config::SendRateHz) {
                ticks_since_ping = 0;
                albw_ping ping = {};
                albw_init_header(&ping.hdr, ALBW_PKT_PING, static_cast<u16>(m_my_id.load()), ++m_seq);
                ping.client_time = static_cast<u64>(nn::os::GetSystemTick().GetInt64Value());
                SendPacket(&ping, sizeof(ping));
            }
            Sleep(period_ms);
        }
    }
}
