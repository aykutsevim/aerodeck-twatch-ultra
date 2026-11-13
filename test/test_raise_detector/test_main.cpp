// core::RaiseDetector: wrist raise from gravity (S7 data-model §3, FR-002). Synthetic 100 Hz sequences.
#include <unity.h>
#include <cmath>
#include "core/RaiseDetector.h"

using namespace core;

void setUp() {}
void tearDown() {}

static constexpr float kD2R = 0.01745329252f;

// Gravity for a pose given as pitch/roll (core::Attitude conventions), scaled like the hub (|g| = 0.5).
struct G {
    float x, y, z;
};
static G pose(float pitchDeg, float rollDeg, float scale = 0.5f)
{
    const float p = pitchDeg * kD2R, r = rollDeg * kD2R;
    // pitch = atan2(-y, sqrt(x²+z²)), roll = atan2(x, z)
    const float y = -std::sin(p);
    const float c = std::cos(p);
    return G{c * std::sin(r) * scale, y * scale, c * std::cos(r) * scale};
}

struct Run {
    RaiseDetector d;
    uint32_t t = 0;
    int fires = 0;
    float noise = 0.0f;
    uint32_t seed = 1;
    float rnd()
    {
        seed = seed * 1103515245u + 12345u;
        return (float((seed >> 16) & 0x7FFF) / 32767.0f * 2.0f - 1.0f) * noise;
    }
    // hold a pose for ms (10 ms steps)
    void hold(float pitch, float roll, uint32_t ms)
    {
        for (uint32_t i = 0; i < ms; i += 10) {
            G g = pose(pitch, roll);
            fires += d.feed(g.x + rnd(), g.y + rnd(), g.z + rnd(), t) ? 1 : 0;
            t += 10;
        }
    }
    // move linearly from (p0,r0) to (p1,r1) over ms
    void move(float p0, float r0, float p1, float r1, uint32_t ms)
    {
        for (uint32_t i = 0; i < ms; i += 10) {
            float f = float(i) / float(ms);
            G g = pose(p0 + (p1 - p0) * f, r0 + (r1 - r0) * f);
            fires += d.feed(g.x + rnd(), g.y + rnd(), g.z + rnd(), t) ? 1 : 0;
            t += 10;
        }
    }
};

// Arm hanging: watch face vertical (roll ≈ 90°). Viewing pose: pitch 20°, roll 0°.
static void test_basic_raise_fires_once()
{
    Run r;
    r.hold(0, 90, 1000);
    r.move(0, 90, 20, 0, 400);
    r.hold(20, 0, 2000);
    TEST_ASSERT_EQUAL(1, r.fires);
}

static void test_already_face_up_never_fires()
{
    Run r;
    r.hold(10, 5, 10000);
    TEST_ASSERT_EQUAL(0, r.fires);
}

static void test_short_away_no_fire()
{
    Run r;
    r.hold(10, 0, 2000);
    r.hold(0, 90, 200);   // away < 300 ms
    r.move(0, 90, 10, 0, 200);
    r.hold(10, 0, 1000);
    TEST_ASSERT_EQUAL(0, r.fires);
}

static void test_slow_rise_no_fire()
{
    Run r;
    r.hold(0, 90, 1000);
    r.move(0, 90, 10, 42, 300);   // leave "away" into the in-between zone …
    r.hold(10, 42, 1500);         // … and linger there > 1 s before reaching the pose
    r.move(10, 42, 20, 0, 200);
    r.hold(20, 0, 1000);
    TEST_ASSERT_EQUAL(0, r.fires);
}

static void test_leaving_before_hold_no_fire()
{
    Run r;
    r.hold(0, 90, 1000);
    r.move(0, 90, 20, 0, 100);    // in the pose only for the last ~40 ms of the move …
    r.hold(20, 0, 50);            // … + 50 ms …
    r.move(20, 0, 0, 90, 100);    // … + the first ~40 ms back: < 150 ms in total
    r.hold(0, 90, 100);
    TEST_ASSERT_EQUAL(0, r.fires);
}

static void test_end_pose_out_of_limits_no_fire()
{
    Run a;
    a.hold(0, 90, 1000);
    a.move(0, 90, 50, 0, 300);   // pitch 50° > 45°
    a.hold(50, 0, 1000);
    TEST_ASSERT_EQUAL(0, a.fires);
    Run b;
    b.hold(0, 90, 1000);
    b.move(0, 90, 10, 40, 300);   // roll 40° > 35°
    b.hold(10, 40, 1000);
    TEST_ASSERT_EQUAL(0, b.fires);
}

static void test_arm_swing_no_fire()
{
    Run r;
    for (int i = 0; i < 5; i++) {
        r.hold(0, 90, 400);
        r.move(0, 90, 0, -90, 160);   // passes through roll 0 for < 100 ms
        r.hold(0, -90, 400);
        r.move(0, -90, 0, 90, 160);
    }
    TEST_ASSERT_EQUAL(0, r.fires);
}

static void test_two_raises_two_fires()
{
    Run r;
    r.hold(0, 90, 1000);
    r.move(0, 90, 20, 0, 400);
    r.hold(20, 0, 1500);
    r.move(20, 0, 0, 90, 400);
    r.hold(0, 90, 1000);
    r.move(0, 90, 25, -5, 400);
    r.hold(25, -5, 1500);
    TEST_ASSERT_EQUAL(2, r.fires);
}

static void test_noise_still_one_fire()
{
    Run r;
    r.noise = 0.025f;   // ±0.05 g at the hub's 0.5 scale
    r.hold(0, 90, 1000);
    r.move(0, 90, 20, 0, 400);
    r.hold(20, 0, 2000);
    TEST_ASSERT_EQUAL(1, r.fires);
}

static void test_arm_down_pitch_raise()
{
    // Arm hanging with the watch face vertical along the other axis (pitch ≈ -80°), then raised.
    Run r;
    r.hold(-80, 0, 800);
    r.move(-80, 0, 15, 0, 500);
    r.hold(15, 0, 1000);
    TEST_ASSERT_EQUAL(1, r.fires);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_basic_raise_fires_once);
    RUN_TEST(test_already_face_up_never_fires);
    RUN_TEST(test_short_away_no_fire);
    RUN_TEST(test_slow_rise_no_fire);
    RUN_TEST(test_leaving_before_hold_no_fire);
    RUN_TEST(test_end_pose_out_of_limits_no_fire);
    RUN_TEST(test_arm_swing_no_fire);
    RUN_TEST(test_two_raises_two_fires);
    RUN_TEST(test_noise_still_one_fire);
    RUN_TEST(test_arm_down_pitch_raise);
    return UNITY_END();
}
