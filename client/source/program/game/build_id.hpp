#pragma once

/*
 * A Link Between Wilds - telling which build of BotW we were loaded into.
 *
 * Every game update moves code and data, so every offset in game_offsets.hpp
 * only holds for one exact build. The build is identified by the GNU build ID
 * that the linker puts in the main executable's .rodata as an ELF note (the
 * same ID the NSO header, Ryujinx and IPS patch file names use).
 *
 * No Switch/SDK dependencies so the same code is unit tested on the host in
 * client/tests/. Reading the running game's memory lives in game_version.cpp.
 */

#include <stddef.h>
#include <stdint.h>

namespace albw::game {

    /* Build IDs are shown as 32 bytes (64 hex digits), zero-padded. */
    constexpr size_t BuildIdSize = 0x20;

    /* BotW Switch 1.0.0 (title 01007EF00011E000, no update): 2094F152836683A3632C6153FD84AB80.
       Source: zeldamods.org/wiki/Versions; decision 0009. */
    constexpr uint8_t Botw100BuildId[BuildIdSize] = {
        0x20, 0x94, 0xF1, 0x52, 0x83, 0x66, 0x83, 0xA3, 0x63, 0x2C, 0x61, 0x53, 0xFD, 0x84, 0xAB, 0x80,
    };

    /* BotW Switch 1.6.0, only used to give a clearer message when someone has the update installed. */
    constexpr uint8_t Botw160BuildId[BuildIdSize] = {
        0x8E, 0x99, 0x78, 0xD5, 0x0B, 0xDD, 0x20, 0xB4, 0xC8, 0x39, 0x5A, 0x10, 0x6C, 0x27, 0xFF, 0xDE,
    };

    /*
     * Finds the first NT_GNU_BUILD_ID note in `data` and copies its ID into
     * `out`, zero-padded to BuildIdSize. Returns false if there is none.
     *
     * Note layout (4-byte aligned): u32 namesz = 4, u32 descsz, u32 type = 3,
     * "GNU\0", then descsz bytes of ID.
     */
    inline bool FindGnuBuildId(const uint8_t* data, size_t size, uint8_t out[BuildIdSize]) {
        auto u32_at = [data](size_t i) {
            return (uint32_t)data[i] | (uint32_t)data[i + 1] << 8 |
                   (uint32_t)data[i + 2] << 16 | (uint32_t)data[i + 3] << 24;
        };
        constexpr size_t HeaderSize = 16;
        for (size_t i = 0; i + HeaderSize <= size; i += 4) {
            if (u32_at(i) != 4 || u32_at(i + 8) != 3)
                continue;
            if (data[i + 12] != 'G' || data[i + 13] != 'N' || data[i + 14] != 'U' || data[i + 15] != 0)
                continue;
            uint32_t len = u32_at(i + 4);
            /* Real IDs are 16 (md5/uuid) or 20 (sha1) bytes; 32 is the most the NSO header holds. */
            if (len < 8 || len > BuildIdSize || i + HeaderSize + len > size)
                continue;
            for (size_t b = 0; b < BuildIdSize; b++)
                out[b] = b < len ? data[i + HeaderSize + b] : 0;
            return true;
        }
        return false;
    }

    inline bool SameBuildId(const uint8_t a[BuildIdSize], const uint8_t b[BuildIdSize]) {
        for (size_t i = 0; i < BuildIdSize; i++) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    /* Writes the first 16 bytes as 32 uppercase hex digits plus NUL (how the IDs are usually quoted). */
    inline void FormatBuildId(const uint8_t id[BuildIdSize], char out[33]) {
        constexpr char Hex[] = "0123456789ABCDEF";
        for (size_t i = 0; i < 16; i++) {
            out[i * 2] = Hex[id[i] >> 4];
            out[i * 2 + 1] = Hex[id[i] & 0xF];
        }
        out[32] = 0;
    }
}
