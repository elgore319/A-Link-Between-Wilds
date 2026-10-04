#pragma once

/*
 * Declarations for the Nintendo SDK socket API (nn::socket), resolved at load
 * time from the game's "sdk" module by exlaunch's dynamic linker.
 *
 * IMPORTANT: these must match the SDK's real signatures exactly, because the
 * C++ mangled names are what get looked up. These follow the older-SDK form
 * (global `struct sockaddr`) used by BotW-era games. If the module aborts on
 * load with an unresolved symbol, open the sdk module in Ghidra, search its
 * exports for "socket", and adjust the parameter types here to match.
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

namespace nn::socket {
    Result Initialize(void* pool, u64 pool_size, u64 alloc_pool_size, s32 concurrency_limit);
    Result Finalize();

    s32  Socket(s32 domain, s32 type, s32 protocol);
    s32  Close(s32 fd);
    s32  Connect(s32 fd, const sockaddr* address, u32 address_len);
    s64  Send(s32 fd, const void* buffer, u64 length, s32 flags);
    s64  Recv(s32 fd, void* buffer, u64 length, s32 flags);
    s32  GetLastErrno();

    u16  InetHtons(u16 host);
    s32  InetAton(const char* str, in_addr* out);
}

namespace albw::net {
    /* BSD constants as used by the Switch. */
    constexpr s32 AfInet      = 2;
    constexpr s32 SockDgram   = 2;
    constexpr s32 IpProtoUdp  = 17;
    /* FreeBSD value; makes a single Recv non-blocking without needing Fcntl. */
    constexpr s32 MsgDontWait = 0x80;
}
