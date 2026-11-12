#pragma once

/*
 * The nn::socket functions the client uses, found at runtime in the game's own
 * modules instead of being imported by the loader (decision 0011).
 *
 * Each function is looked up under every mangled name it has had across SDK
 * versions (old SDKs use plain ints and a global `sockaddr`; newer ones use
 * nn::socket enums and nn::socket::SockAddr; the calling convention is the same
 * either way). If one can't be found, Resolve() says which, the client logs it,
 * and networking stays off while the game keeps running.
 */

#include <common.hpp>
#include "nn_socket.hpp"

namespace albw::net {

    struct SocketApi {
        /* Optional: if the game has no 4-argument Initialize we try without it. */
        Result (*Initialize)(void* pool, u64 pool_size, u64 alloc_pool_size, s32 concurrency) = nullptr;
        /* Required. */
        s32 (*Socket)(s32 domain, s32 type, s32 protocol) = nullptr;
        s32 (*Connect)(s32 fd, const sockaddr* address, u32 address_len) = nullptr;
        s64 (*Send)(s32 fd, const void* buffer, u64 length, s32 flags) = nullptr;
        s64 (*Recv)(s32 fd, void* buffer, u64 length, s32 flags) = nullptr;
        /* Optional: only used to put an error number in the log. */
        s32 (*GetLastErrno)() = nullptr;

        s32 LastErrno() const { return GetLastErrno ? GetLastErrno() : -1; }
    };

    /* Fills `out` from the game's modules. Returns nullptr when every required
       function was found, otherwise the name of the first missing one. Logs which
       module and mangled name each function came from. */
    const char* ResolveSocketApi(SocketApi& out);

    /* Logs every nn::socket function the game exports (up to a cap), so a test
       report shows the real names if ours don't match. */
    void LogSocketExports();
}
