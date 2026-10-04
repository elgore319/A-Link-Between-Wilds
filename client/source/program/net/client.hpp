#pragma once

#include <common.hpp>
#include <atomic>
#include "albw_protocol.h"

namespace albw::net {

    /* What we know about one other player. */
    struct RemotePlayer {
        bool     active = false;
        char     name[ALBW_NAME_LEN + 1] = {};
        albw_player_state last = {};   /* newest state received   */
        u32      last_seq = 0;
        u64      packets = 0;
    };

    enum class Status : u8 {
        Off,
        Connecting,
        Connected,
        Rejected,
        Error,
    };

    /*
     * Owns the UDP socket and a background thread that:
     *   - joins the server (HELLO -> WELCOME), retrying until it answers
     *   - sends our latest state SendRateHz times per second
     *   - receives other players' states / join / leave packets
     *   - pings once a second so the server doesn't time us out
     *
     * The game thread only touches it through SetLocalState() and GetRemote(),
     * both guarded by a tiny spinlock so neither side ever blocks for long.
     */
    class Client {
        public:
            /* Starts the network thread. Call once from exl_main. */
            void Start();

            /* Called from the game thread (player hook) every frame. */
            void SetLocalState(const float pos[3], float rot_y, u32 game_tick);

            /* Copies out remote player `id` (0..ALBW_MAX_PLAYERS-1). False if absent. */
            bool GetRemote(int id, RemotePlayer& out);

            Status GetStatus() const { return m_status.load(); }
            int    GetMyId() const { return m_my_id.load(); }

            /* With no game hook yet, have the net thread invent a player walking
               in a circle so the whole network path can be tested. */
            void EnableTestPattern() { m_test_pattern = true; }

        private:
            static void ThreadEntry(void* arg);
            void Run();

            bool OpenSocket();
            void Join();
            void Pump();
            void HandlePacket(const u8* data, size_t size);
            void SendState();
            void SendPacket(const void* data, size_t size);
            void StepTestPattern();

            void Lock();
            void Unlock();

            s32 m_fd = -1;
            u32 m_seq = 0;
            std::atomic<Status> m_status { Status::Off };
            std::atomic<int>    m_my_id { ALBW_INVALID_PLAYER };
            bool  m_test_pattern = false;
            float m_test_dir[2] = { 5.0f, 0.0f };
            u32   m_test_tick = 0;

            /* Shared between threads, guarded by m_lock. */
            std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
            float m_pos[3] = {};
            float m_prev_pos[3] = {};
            float m_rot_y = 0;
            u32   m_game_tick = 0;
            bool  m_have_local = false;
            RemotePlayer m_remote[ALBW_MAX_PLAYERS];
    };

    Client& GetClient();
}
