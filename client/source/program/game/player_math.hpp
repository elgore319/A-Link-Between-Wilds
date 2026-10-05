#pragma once

/*
 * A Link Between Wilds - turning the player actor's matrix into what we send.
 *
 * No Switch/SDK dependencies (no nn::, no libc/libm) so the same code is unit
 * tested on the host in client/tests/. Memory access lives in player_reader.cpp.
 */

#include <stddef.h>
#include <stdint.h>

namespace albw::game {

    struct PlayerSample {
        float pos[3];   /* world position; Y is height */
        float yaw;      /* radians, rotation about +Y, range (-pi, pi] */
    };

    namespace math {

        constexpr float Pi = 3.14159265358979f;

        inline bool IsFinite(float v) {
            /* NaN fails v == v; +-inf gives v - v == NaN. */
            return v == v && (v - v) == 0.0f;
        }

        inline float Abs(float v) { return v < 0 ? -v : v; }

        /*
         * atan2 without libm. Range-reduced to [0, 1], then a minimax polynomial
         * for atan on [0, 1]; max error about 1e-5 rad (checked in the tests),
         * which is far below anything visible.
         */
        inline float Atan2(float y, float x) {
            float ax = Abs(x), ay = Abs(y);
            if (ax == 0.0f && ay == 0.0f)
                return 0.0f;
            bool swap = ay > ax;
            float t = swap ? ax / ay : ay / ax;          /* in [0, 1] */
            float t2 = t * t;
            float r = t * (0.99997726f + t2 * (-0.33262347f + t2 * (0.19354346f +
                      t2 * (-0.11643287f + t2 * (0.05265332f + t2 * -0.01172120f)))));
            if (swap) r = Pi / 2 - r;
            if (x < 0) r = Pi - r;
            if (y < 0) r = -r;
            return r;
        }
    }

    /*
     * Sanity limits for a real player transform. Hyrule is roughly 12000 x 10000
     * units centred on the origin, so anything far outside is garbage (e.g. a
     * stale pointer into reused memory during a loading screen).
     */
    constexpr float MaxAbsCoord = 20000.0f;

    /*
     * Reads a sead::Matrix34f (3 rows x 4 floats, row-major, translation in
     * column 3) and fills `out`. Returns false if the matrix doesn't look like a
     * real, unscaled actor transform, in which case `out` is untouched.
     *
     * Yaw = atan2(m[0][2], m[2][2]): for a pure rotation by t about Y,
     * m[0][2] = sin t and m[2][2] = cos t. (Which way is "forward" in BotW is
     * still to be confirmed in-game; see docs/research/decomp-mapping.md.)
     */
    inline bool SampleFromMtx34(const float m[12], PlayerSample& out) {
        using namespace math;
        for (int i = 0; i < 12; i++) {
            if (!IsFinite(m[i])) return false;
        }

        float x = m[3], y = m[7], z = m[11];
        if (Abs(x) > MaxAbsCoord || Abs(y) > MaxAbsCoord || Abs(z) > MaxAbsCoord)
            return false;
        /* A freshly constructed actor has an identity matrix: exactly 0,0,0. */
        if (x == 0.0f && y == 0.0f && z == 0.0f)
            return false;

        /* Each rotation row should be (close to) unit length. Garbage won't be. */
        for (int r = 0; r < 3; r++) {
            const float* row = m + r * 4;
            float len2 = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
            if (len2 < 0.8f || len2 > 1.25f) return false;
        }

        out.pos[0] = x;
        out.pos[1] = y;
        out.pos[2] = z;
        out.yaw = Atan2(m[2], m[10]);
        return true;
    }
}
