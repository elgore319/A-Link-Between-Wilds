/*
 * A Link Between Wilds - wire protocol
 *
 * Shared between the Switch client module (C++) and anything else written in C/C++.
 * The Python server mirrors these layouts in server/protocol.py; keep them in sync
 * and bump ALBW_PROTOCOL_VERSION whenever a layout changes.
 *
 * All packets are single UDP datagrams, little-endian (native on both the Switch
 * and x86/ARM PCs), with no padding.
 */
#ifndef ALBW_PROTOCOL_H
#define ALBW_PROTOCOL_H

#include <stdint.h>

#define ALBW_MAGIC            0x57424C41u /* "ALBW" as little-endian bytes */
#define ALBW_PROTOCOL_VERSION 1
#define ALBW_DEFAULT_PORT     55420
#define ALBW_MAX_PLAYERS      4           /* a Four Swords nod */
#define ALBW_NAME_LEN         16
#define ALBW_INVALID_PLAYER   0xFFFF

enum albw_packet_type {
    ALBW_PKT_HELLO        = 1, /* client -> server: I want to join          */
    ALBW_PKT_WELCOME      = 2, /* server -> client: you joined, here's your id */
    ALBW_PKT_REJECT       = 3, /* server -> client: can't join (full, version) */
    ALBW_PKT_PLAYER_STATE = 4, /* client -> server -> other clients          */
    ALBW_PKT_PLAYER_JOIN  = 5, /* server -> clients: someone joined          */
    ALBW_PKT_PLAYER_LEAVE = 6, /* server -> clients: someone left/timed out  */
    ALBW_PKT_PING         = 7, /* client -> server, keeps the session alive  */
    ALBW_PKT_PONG         = 8, /* server -> client                           */
    ALBW_PKT_BYE          = 9  /* client -> server: leaving cleanly          */
};

enum albw_reject_reason {
    ALBW_REJECT_FULL        = 1,
    ALBW_REJECT_BAD_VERSION = 2
};

#pragma pack(push, 1)

typedef struct albw_header {
    uint32_t magic;      /* ALBW_MAGIC                                       */
    uint8_t  version;    /* ALBW_PROTOCOL_VERSION                            */
    uint8_t  type;       /* albw_packet_type                                 */
    uint16_t player_id;  /* sender's id; ALBW_INVALID_PLAYER before WELCOME  */
    uint32_t seq;        /* per-sender counter, used to drop stale packets   */
} albw_header;

typedef struct albw_hello {
    albw_header hdr;
    char name[ALBW_NAME_LEN]; /* not necessarily NUL-terminated */
} albw_hello;

typedef struct albw_welcome {
    albw_header hdr;          /* hdr.player_id = the id you were assigned */
    uint8_t max_players;
    uint8_t player_count;
} albw_welcome;

typedef struct albw_reject {
    albw_header hdr;
    uint8_t reason;           /* albw_reject_reason */
} albw_reject;

/* Everything needed to draw another player. Grows as the mod does. */
typedef struct albw_player_state {
    albw_header hdr;
    float    pos[3];          /* world position, game units                */
    float    rot_y;           /* facing, radians                           */
    float    vel[3];          /* used for extrapolation between packets    */
    uint32_t anim_id;         /* game animation identifier (TBD)           */
    uint32_t flags;           /* ALBW_STATE_* bits                         */
    uint32_t game_tick;       /* sender's frame counter, for interpolation */
} albw_player_state;

enum albw_state_flags {
    ALBW_STATE_GROUNDED  = 1u << 0,
    ALBW_STATE_GLIDING   = 1u << 1,
    ALBW_STATE_SWIMMING  = 1u << 2,
    ALBW_STATE_CLIMBING  = 1u << 3,
    ALBW_STATE_IN_MENU   = 1u << 4
};

typedef struct albw_player_join {
    albw_header hdr;          /* hdr.player_id = the player who joined */
    char name[ALBW_NAME_LEN];
} albw_player_join;

typedef struct albw_player_leave {
    albw_header hdr;          /* hdr.player_id = the player who left */
} albw_player_leave;

typedef struct albw_ping {
    albw_header hdr;
    uint64_t client_time;     /* echoed back in PONG for RTT measurement */
} albw_ping;

#pragma pack(pop)

/* Layout checks: these sizes are mirrored in server/protocol.py. */
#ifdef __cplusplus
#define ALBW_STATIC_ASSERT(c, m) static_assert(c, m)
#else
#define ALBW_STATIC_ASSERT(c, m) _Static_assert(c, m)
#endif
ALBW_STATIC_ASSERT(sizeof(albw_header) == 12, "albw_header size");
ALBW_STATIC_ASSERT(sizeof(albw_hello) == 28, "albw_hello size");
ALBW_STATIC_ASSERT(sizeof(albw_welcome) == 14, "albw_welcome size");
ALBW_STATIC_ASSERT(sizeof(albw_reject) == 13, "albw_reject size");
ALBW_STATIC_ASSERT(sizeof(albw_player_state) == 52, "albw_player_state size");
ALBW_STATIC_ASSERT(sizeof(albw_player_join) == 28, "albw_player_join size");
ALBW_STATIC_ASSERT(sizeof(albw_player_leave) == 12, "albw_player_leave size");
ALBW_STATIC_ASSERT(sizeof(albw_ping) == 20, "albw_ping size");

static inline void albw_init_header(albw_header* h, uint8_t type, uint16_t player_id, uint32_t seq) {
    h->magic = ALBW_MAGIC;
    h->version = ALBW_PROTOCOL_VERSION;
    h->type = type;
    h->player_id = player_id;
    h->seq = seq;
}

#endif /* ALBW_PROTOCOL_H */
