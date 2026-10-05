/*
 * Host-side tests for client/source/program/config/config_parser.hpp.
 *
 * The parser has no Switch dependencies, so it is tested here with the normal
 * system compiler instead of on hardware. Build and run:
 *
 *     g++ -std=c++20 -Wall -Wextra -Werror -I../source -I../../protocol test_config_parser.cpp -o test_config_parser && ./test_config_parser
 *
 * (CI runs exactly this; see the "client-tests" job in .github/workflows/ci.yml.)
 */
#include "program/config/config_parser.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using albw::config::Parse;
using albw::config::ParseStats;
using albw::config::Settings;

namespace {
    int g_failed = 0;
    int g_checks = 0;

    #define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

    struct Warning { int line; std::string msg; };

    void Collect(void* user, int line, const char* msg) {
        static_cast<std::vector<Warning>*>(user)->push_back({ line, msg });
    }

    Settings Defaults() {
        Settings s;
        std::strcpy(s.server_ip, "127.0.0.1");
        s.server_port = 55420;
        std::strcpy(s.player_name, "Link");
        return s;
    }

    struct Result { Settings s; ParseStats stats; std::vector<Warning> warnings; };

    Result Run(const std::string& text) {
        Result r { Defaults(), {}, {} };
        r.stats = Parse(text.data(), text.size(), r.s, Collect, &r.warnings);
        return r;
    }

    void TestFullFile() {
        auto r = Run(
            "# A Link Between Wilds\n"
            "[albw]\n"
            "server_ip = 192.168.1.50\n"
            "server_port = 6000\n"
            "player_name = Linkle\n");
        CHECK(std::strcmp(r.s.server_ip, "192.168.1.50") == 0);
        CHECK(r.s.server_port == 6000);
        CHECK(std::strcmp(r.s.player_name, "Linkle") == 0);
        CHECK(r.stats.applied == 3);
        CHECK(r.stats.warnings == 0);
    }

    void TestEmptyFileKeepsDefaults() {
        auto r = Run("");
        CHECK(std::strcmp(r.s.server_ip, "127.0.0.1") == 0);
        CHECK(r.s.server_port == 55420);
        CHECK(std::strcmp(r.s.player_name, "Link") == 0);
        CHECK(r.stats.applied == 0 && r.stats.warnings == 0);
    }

    void TestPartialFileKeepsOtherDefaults() {
        auto r = Run("player_name=Zelda");
        CHECK(std::strcmp(r.s.player_name, "Zelda") == 0);
        CHECK(std::strcmp(r.s.server_ip, "127.0.0.1") == 0);
        CHECK(r.s.server_port == 55420);
    }

    void TestWindowsNotepadFile() {
        /* UTF-8 BOM, CRLF endings, tabs, mixed-case keys, no trailing newline. */
        auto r = Run("\xEF\xBB\xBF; made in Notepad\r\n\tSERVER_IP\t=\t10.0.0.2  \r\nServer_Port=55421\r\nplayer_name = Mipha");
        CHECK(std::strcmp(r.s.server_ip, "10.0.0.2") == 0);
        CHECK(r.s.server_port == 55421);
        CHECK(std::strcmp(r.s.player_name, "Mipha") == 0);
        CHECK(r.stats.warnings == 0);
    }

    void TestNameWithSpacesAndMaxLength() {
        auto r = Run("player_name = Hero of Hyrule!\n");
        CHECK(std::strcmp(r.s.player_name, "Hero of Hyrule!") == 0);
        auto r16 = Run("player_name = 0123456789abcdef\n");
        CHECK(std::strcmp(r16.s.player_name, "0123456789abcdef") == 0);
        CHECK(r16.stats.warnings == 0);
    }

    void TestBadValuesAreRejectedAndReported() {
        auto r = Run(
            "server_ip = example.com\n"        /* 1: hostname */
            "server_ip = 256.1.1.1\n"          /* 2: part out of range */
            "server_ip = 1.2.3\n"              /* 3: too few parts */
            "server_ip = 1.2.3.4.5\n"          /* 4: too many parts */
            "server_ip = 010.0.0.1\n"          /* 5: leading zero (octal trap) */
            "server_port = 0\n"                /* 6 */
            "server_port = 70000\n"            /* 7 */
            "server_port = 55a\n"              /* 8 */
            "player_name = 0123456789abcdefg\n"/* 9: 17 chars */
            "player_name =\n"                  /* 10: empty */
            "player_name = Lïnk\n"             /* 11: non-ASCII */
            "colour = green\n"                 /* 12: unknown key */
            "just some words\n"                /* 13: no '=' */
            "[broken\n");                      /* 14: bad section */
        CHECK(r.stats.applied == 0);
        CHECK(r.stats.warnings == 14);
        CHECK(r.warnings.size() == 14);
        for (size_t i = 0; i < r.warnings.size(); i++)
            CHECK(r.warnings[i].line == static_cast<int>(i) + 1);
        /* Nothing changed. */
        CHECK(std::strcmp(r.s.server_ip, "127.0.0.1") == 0);
        CHECK(r.s.server_port == 55420);
        CHECK(std::strcmp(r.s.player_name, "Link") == 0);
    }

    void TestBadLineDoesNotStopLaterLines() {
        auto r = Run("server_port = nope\nserver_port = 1234\n");
        CHECK(r.s.server_port == 1234);
        CHECK(r.stats.applied == 1 && r.stats.warnings == 1);
        CHECK(r.warnings.size() == 1 && r.warnings[0].line == 1);
    }

    void TestLaterValueWins() {
        auto r = Run("server_ip = 1.1.1.1\nserver_ip = 2.2.2.2\n");
        CHECK(std::strcmp(r.s.server_ip, "2.2.2.2") == 0);
    }

    void TestBoundaryValues() {
        auto r = Run("server_ip = 255.255.255.255\nserver_port = 65535\n");
        CHECK(std::strcmp(r.s.server_ip, "255.255.255.255") == 0);
        CHECK(r.s.server_port == 65535);
        auto r0 = Run("server_ip = 0.0.0.0\nserver_port = 1\n");
        CHECK(std::strcmp(r0.s.server_ip, "0.0.0.0") == 0);
        CHECK(r0.s.server_port == 1);
    }

    void TestValueContainingEquals() {
        /* Only the first '=' splits; the rest belongs to the value. */
        auto r = Run("player_name = a=b\n");
        CHECK(std::strcmp(r.s.player_name, "a=b") == 0);
    }

    void TestShorterValueClearsOldOne() {
        auto r = Run("player_name = Ganondorf\nplayer_name = Ike\n");
        CHECK(std::strcmp(r.s.player_name, "Ike") == 0);
        CHECK(r.s.player_name[4] == '\0' && r.s.player_name[8] == '\0');
    }

    void TestNoCallbackIsFine() {
        Settings s = Defaults();
        const char text[] = "nonsense\nserver_port = 7\n";
        ParseStats st = Parse(text, sizeof(text) - 1, s);
        CHECK(s.server_port == 7);
        CHECK(st.warnings == 1);
    }

    void TestNotNulTerminatedInput() {
        /* The module reads the file into a buffer without a terminator. */
        char buf[] = { 's','e','r','v','e','r','_','p','o','r','t','=','9','9','X' };
        Settings s = Defaults();
        Parse(buf, sizeof(buf) - 1, s); /* exclude the 'X' */
        CHECK(s.server_port == 99);
    }
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        { "full file", TestFullFile },
        { "empty file keeps defaults", TestEmptyFileKeepsDefaults },
        { "partial file keeps other defaults", TestPartialFileKeepsOtherDefaults },
        { "Windows Notepad file", TestWindowsNotepadFile },
        { "name with spaces / max length", TestNameWithSpacesAndMaxLength },
        { "bad values rejected and reported", TestBadValuesAreRejectedAndReported },
        { "bad line doesn't stop later lines", TestBadLineDoesNotStopLaterLines },
        { "later value wins", TestLaterValueWins },
        { "boundary values", TestBoundaryValues },
        { "value containing '='", TestValueContainingEquals },
        { "shorter value clears old one", TestShorterValueClearsOldOne },
        { "no callback", TestNoCallbackIsFine },
        { "input without terminator", TestNotNulTerminatedInput },
    };
    for (auto& t : tests) {
        int before = g_failed;
        t.fn();
        std::printf("%s %s\n", g_failed == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
