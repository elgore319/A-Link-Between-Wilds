#pragma once

/*
 * Socket types and constants for the Switch's nn::socket API.
 *
 * The functions themselves are not declared here: they are found at runtime by
 * socket_api.cpp (decision 0011), so a wrong guess about their exact signature
 * shows up as a log line instead of a crash.
 *
 * Layouts follow the Switch's BSD-derived stack: sockaddr has a length byte
 * before the family byte.
 */

#include <common.hpp>

struct in_addr {
    u32 s_addr; /* network byte order */
};

struct sockaddr {
    u8   sa_len;
    u8   sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    u8      sin_len;
    u8      sin_family;
    u16     sin_port;   /* network byte order */
    in_addr sin_addr;
    u8      sin_zero[8];
};
static_assert(sizeof(sockaddr_in) == sizeof(sockaddr));

namespace albw::net {
    /* BSD constants as used by the Switch. */
    constexpr s32 AfInet      = 2;
    constexpr s32 SockDgram   = 2;
    constexpr s32 IpProtoUdp  = 17;
    /* FreeBSD value; makes a single Recv non-blocking without needing Fcntl. */
    constexpr s32 MsgDontWait = 0x80;
}
