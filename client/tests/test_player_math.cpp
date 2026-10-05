/*
 * Host-side tests for client/source/program/game/player_math.hpp.
 *
 *     g++ -std=c++20 -Wall -Wextra -Werror -I../source -I../../protocol test_player_math.cpp -o test_player_math && ./test_player_math
 */
#include "program/game/player_math.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

using albw::game::PlayerSample;
using albw::game::SampleFromMtx34;
namespace m = albw::game::math;

namespace {
    int g_failed = 0;
    int g_checks = 0;

    #define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

    bool Near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

    double AngleDiff(double a, double b) {
        double d = std::fmod(std::fabs(a - b), 2 * M_PI);
        return d > M_PI ? 2 * M_PI - d : d;
    }

    /* sead::Matrix34f memory layout: rows of 4, translation in column 3. */
    void MakeYawMtx(float out[12], double yaw, float x, float y, float z, float scale = 1.0f) {
        float c = static_cast<float>(std::cos(yaw)) * scale, s = static_cast<float>(std::sin(yaw)) * scale;
        float mtx[12] = {
             c,    0,     s, x,
             0, scale,    0, y,
            -s,    0,     c, z,
        };
        std::memcpy(out, mtx, sizeof(mtx));
    }

    void TestAtan2MatchesLibm() {
        double max_err = 0;
        for (int i = 0; i <= 100000; i++) {
            double a = -M_PI + 2 * M_PI * i / 100000;
            for (double r : { 1e-3, 1.0, 5000.0 }) {
                float y = static_cast<float>(r * std::sin(a)), x = static_cast<float>(r * std::cos(a));
                double err = AngleDiff(m::Atan2(y, x), std::atan2(y, x));
                if (err > max_err) max_err = err;
            }
        }
        CHECK(max_err < 1e-5);
    }

    void TestAtan2Axes() {
        CHECK(Near(m::Atan2(0, 1), 0, 1e-6));
        CHECK(Near(m::Atan2(1, 0), M_PI / 2, 1e-6));
        CHECK(Near(m::Atan2(0, -1), M_PI, 1e-6));
        CHECK(Near(m::Atan2(-1, 0), -M_PI / 2, 1e-6));
        CHECK(m::Atan2(0, 0) == 0.0f);
    }

    void TestIsFinite() {
        CHECK(m::IsFinite(0.0f) && m::IsFinite(-123.5f) && m::IsFinite(3e38f));
        CHECK(!m::IsFinite(std::numeric_limits<float>::infinity()));
        CHECK(!m::IsFinite(-std::numeric_limits<float>::infinity()));
        CHECK(!m::IsFinite(std::numeric_limits<float>::quiet_NaN()));
    }

    void TestExtractsPositionAndYaw() {
        for (double yaw : { 0.0, 0.5, 1.5707963, 2.5, 3.1, -0.7, -2.9 }) {
            float mtx[12];
            MakeYawMtx(mtx, yaw, -1234.5f, 245.25f, 2000.0f);
            PlayerSample s {};
            CHECK(SampleFromMtx34(mtx, s));
            CHECK(s.pos[0] == -1234.5f && s.pos[1] == 245.25f && s.pos[2] == 2000.0f);
            CHECK(AngleDiff(s.yaw, yaw) < 1e-5);
        }
    }

    void TestRejectsIdentityAtOrigin() {
        /* A just-constructed actor: identity rotation, translation exactly 0. */
        float mtx[12];
        MakeYawMtx(mtx, 0, 0, 0, 0);
        PlayerSample s {};
        CHECK(!SampleFromMtx34(mtx, s));
    }

    void TestRejectsNonFinite() {
        float mtx[12];
        MakeYawMtx(mtx, 0.3, 10, 20, 30);
        mtx[5] = std::numeric_limits<float>::quiet_NaN();
        PlayerSample s {};
        CHECK(!SampleFromMtx34(mtx, s));
        MakeYawMtx(mtx, 0.3, 10, 20, 30);
        mtx[11] = std::numeric_limits<float>::infinity();
        CHECK(!SampleFromMtx34(mtx, s));
    }

    void TestRejectsOutOfWorld() {
        float mtx[12];
        PlayerSample s {};
        MakeYawMtx(mtx, 0, 25000, 0, 0);
        CHECK(!SampleFromMtx34(mtx, s));
        MakeYawMtx(mtx, 0, 0, -25000, 0);
        CHECK(!SampleFromMtx34(mtx, s));
        MakeYawMtx(mtx, 0, 19999, 1, -19999);
        CHECK(SampleFromMtx34(mtx, s));
    }

    void TestRejectsScaledOrGarbageRotation() {
        float mtx[12];
        PlayerSample s {};
        MakeYawMtx(mtx, 1.0, 5, 5, 5, 2.0f);   /* scaled x2: not the player */
        CHECK(!SampleFromMtx34(mtx, s));
        MakeYawMtx(mtx, 1.0, 5, 5, 5, 0.5f);
        CHECK(!SampleFromMtx34(mtx, s));
        /* Typical garbage: small integers / pointers reinterpreted as floats. */
        float junk[12] = { 1e-40f, 0, 0, 100, 0, 0, 0, 200, 0, 0, 0, 300 };
        CHECK(!SampleFromMtx34(junk, s));
    }

    void TestFailureLeavesOutputUntouched() {
        float mtx[12];
        MakeYawMtx(mtx, 0, 0, 0, 0);
        PlayerSample s { { 1, 2, 3 }, 4 };
        CHECK(!SampleFromMtx34(mtx, s));
        CHECK(s.pos[0] == 1 && s.pos[1] == 2 && s.pos[2] == 3 && s.yaw == 4);
    }
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        { "atan2 matches libm", TestAtan2MatchesLibm },
        { "atan2 on axes", TestAtan2Axes },
        { "IsFinite", TestIsFinite },
        { "extracts position and yaw", TestExtractsPositionAndYaw },
        { "rejects identity at origin", TestRejectsIdentityAtOrigin },
        { "rejects NaN/inf", TestRejectsNonFinite },
        { "rejects out-of-world coords", TestRejectsOutOfWorld },
        { "rejects scaled/garbage rotation", TestRejectsScaledOrGarbageRotation },
        { "failure leaves output untouched", TestFailureLeavesOutputUntouched },
    };
    for (auto& t : tests) {
        int before = g_failed;
        t.fn();
        std::printf("%s %s\n", g_failed == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
