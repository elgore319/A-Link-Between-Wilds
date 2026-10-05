#pragma once

/*
 * A Link Between Wilds - config.ini parser.
 *
 * Deliberately free of any Switch/SDK dependency (no nn::, no libc calls) so the
 * exact same code is compiled into the module and into the host-side unit tests
 * in client/tests/. Keep it that way: if this file needs something from the SDK,
 * it belongs in config_file.cpp instead.
 *
 * Format (see docs/configuration.md):
 *
 *     # comment            ; also a comment
 *     [albw]               <- section headers are accepted and ignored
 *     server_ip   = 192.168.1.50
 *     server_port = 55420
 *     player_name = Linkle
 *
 * Rules: one key per line, whitespace around keys/values is trimmed, keys are
 * case-insensitive, a UTF-8 BOM (Notepad adds one) and CRLF line endings are fine.
 * An invalid or unknown line is reported through the warning callback and skipped;
 * the setting it would have changed keeps its previous (default) value.
 */

#include <stddef.h>
#include <stdint.h>
#include "albw_protocol.h"

namespace albw::config {

    /* "255.255.255.255" plus the terminator. */
    constexpr size_t ServerIpMax = 15;

    struct Settings {
        char     server_ip[ServerIpMax + 1] = {};
        uint16_t server_port = 0;
        char     player_name[ALBW_NAME_LEN + 1] = {};
    };

    /* Called once per problem. `line` is 1-based. `msg` is a static string. */
    using WarnFn = void (*)(void* user, int line, const char* msg);

    struct ParseStats {
        int applied = 0;   /* keys that changed a setting      */
        int warnings = 0;  /* lines that were skipped/rejected */
    };

    namespace detail {

        inline bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f'; }
        inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
        inline char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

        /* A [begin, end) view into the file buffer. */
        struct Span {
            const char* b;
            const char* e;
            size_t Len() const { return static_cast<size_t>(e - b); }
            bool Empty() const { return b == e; }
        };

        inline Span Trim(Span s) {
            while (s.b < s.e && IsSpace(*s.b)) s.b++;
            while (s.e > s.b && IsSpace(s.e[-1])) s.e--;
            return s;
        }

        inline bool KeyIs(Span key, const char* name) {
            const char* p = key.b;
            for (; *name != '\0'; name++, p++) {
                if (p == key.e || Lower(*p) != *name) return false;
            }
            return p == key.e;
        }

        /* Decimal integer in [lo, hi], digits only. */
        inline bool ParseUInt(Span s, uint32_t lo, uint32_t hi, uint32_t& out) {
            if (s.Empty() || s.Len() > 10) return false;
            uint64_t v = 0;
            for (const char* p = s.b; p < s.e; p++) {
                if (!IsDigit(*p)) return false;
                v = v * 10 + static_cast<uint64_t>(*p - '0');
            }
            if (v < lo || v > hi) return false;
            out = static_cast<uint32_t>(v);
            return true;
        }

        /* Strict dotted-quad IPv4: four 0..255 parts, no leading '+', no hostnames. */
        inline bool IsIpv4(Span s) {
            if (s.Empty() || s.Len() > ServerIpMax) return false;
            int parts = 0;
            const char* p = s.b;
            while (true) {
                const char* start = p;
                while (p < s.e && IsDigit(*p)) p++;
                uint32_t part;
                if (p - start > 3 || !ParseUInt(Span { start, p }, 0, 255, part)) return false;
                /* inet_aton reads "010" as octal 8; refuse leading zeros rather than surprise anyone. */
                if (p - start > 1 && *start == '0') return false;
                parts++;
                if (p == s.e) break;
                if (*p != '.' || parts == 4) return false;
                p++;
            }
            return parts == 4;
        }

        /* Player names travel in a fixed 16-byte field and end up in other players'
           logs and UIs, so keep them to printable ASCII. */
        inline bool IsValidName(Span s) {
            if (s.Empty() || s.Len() > ALBW_NAME_LEN) return false;
            for (const char* p = s.b; p < s.e; p++) {
                if (*p < 0x20 || *p > 0x7e) return false;
            }
            return true;
        }

        inline void CopySpan(char* dst, size_t cap, Span s) {
            size_t n = s.Len() < cap - 1 ? s.Len() : cap - 1;
            for (size_t i = 0; i < n; i++) dst[i] = s.b[i];
            for (size_t i = n; i < cap; i++) dst[i] = '\0';
        }
    }

    /*
     * Applies the settings found in `text` on top of `inout`, which should hold the
     * defaults. Never fails as a whole: bad lines are skipped and reported.
     */
    inline ParseStats Parse(const char* text, size_t len, Settings& inout, WarnFn warn = nullptr, void* user = nullptr) {
        using namespace detail;
        ParseStats stats;
        auto Warn = [&](int line, const char* msg) {
            stats.warnings++;
            if (warn) warn(user, line, msg);
        };

        const char* p = text;
        const char* end = text + len;
        if (len >= 3 && static_cast<uint8_t>(p[0]) == 0xEF && static_cast<uint8_t>(p[1]) == 0xBB && static_cast<uint8_t>(p[2]) == 0xBF)
            p += 3;

        for (int line_no = 1; p < end; line_no++) {
            const char* line_end = p;
            while (line_end < end && *line_end != '\n') line_end++;
            Span line = Trim(Span { p, line_end });
            p = line_end < end ? line_end + 1 : end;

            if (line.Empty() || *line.b == '#' || *line.b == ';')
                continue;
            if (*line.b == '[') {
                if (line.e[-1] != ']') Warn(line_no, "malformed section header");
                continue;
            }

            const char* eq = line.b;
            while (eq < line.e && *eq != '=') eq++;
            if (eq == line.e) {
                Warn(line_no, "expected 'key = value'");
                continue;
            }
            Span key = Trim(Span { line.b, eq });
            Span value = Trim(Span { eq + 1, line.e });

            if (KeyIs(key, "server_ip")) {
                if (!IsIpv4(value)) { Warn(line_no, "server_ip must be an IPv4 address like 192.168.1.50"); continue; }
                CopySpan(inout.server_ip, sizeof(inout.server_ip), value);
            } else if (KeyIs(key, "server_port")) {
                uint32_t port;
                if (!ParseUInt(value, 1, 65535, port)) { Warn(line_no, "server_port must be a number from 1 to 65535"); continue; }
                inout.server_port = static_cast<uint16_t>(port);
            } else if (KeyIs(key, "player_name")) {
                if (!IsValidName(value)) { Warn(line_no, "player_name must be 1-16 printable ASCII characters"); continue; }
                CopySpan(inout.player_name, sizeof(inout.player_name), value);
            } else {
                Warn(line_no, "unknown key");
                continue;
            }
            stats.applied++;
        }
        return stats;
    }
}
