#include <unity.h>
#include <cmath>
#include "core/Attitude.h"

using core::Attitude;
using core::AttitudeOut;
using core::AxisMap;

void setUp() {}
void tearDown() {}

static const float kD2R = 3.14159265f / 180.0f;

static AttitudeOut settle(Attitude& a, float x, float y, float z, int n = 20)
{
    AttitudeOut o{};
    for (int i = 0; i < n; i++) {
        o = a.update(x, y, z);
    }
    return o;
}

static void test_flat_is_level()
{
    Attitude a;
    AttitudeOut o = settle(a, 0, 0, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, o.pitchDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, o.rollDeg);
}

static void test_roll_30()
{
    Attitude a;
    AttitudeOut o = settle(a, std::sin(30 * kD2R), 0, std::cos(30 * kD2R));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, o.rollDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, o.pitchDeg);
}

static void test_pitch_10_nose_up()
{
    Attitude a;
    AttitudeOut o = settle(a, 0, -std::sin(10 * kD2R), std::cos(10 * kD2R));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, o.pitchDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, o.rollDeg);
}

static void test_vertical_holds_roll()
{
    Attitude a;
    settle(a, std::sin(20 * kD2R), 0, std::cos(20 * kD2R));   // roll 20
    // Pitch to 85° nose-up with a sideways component that would otherwise give a wild roll
    AttitudeOut o = settle(a, 0.05f, -std::sin(85 * kD2R), 0.01f);
    TEST_ASSERT_TRUE(o.pitchDeg > 80.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, o.rollDeg);
}

static void test_ema_converges_quickly()
{
    Attitude a;
    settle(a, 0, 0, 1);
    AttitudeOut o{};
    for (int i = 0; i < 5; i++) {
        o = a.update(std::sin(30 * kD2R), 0, std::cos(30 * kD2R));
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.0f, o.rollDeg);   // 1-0.5^5 = 97 % of the step
}

static void test_roll_wrap_does_not_jump()
{
    Attitude a;
    settle(a, std::sin(179 * kD2R), 0, std::cos(179 * kD2R));   // roll 179
    AttitudeOut o = a.update(std::sin(-179 * kD2R), 0, std::cos(-179 * kD2R));   // -179 (2° away)
    float r = o.rollDeg;
    TEST_ASSERT_TRUE(std::fabs(r) > 178.0f);   // stays near ±180, never swings through 0
}

static void test_reset()
{
    Attitude a;
    settle(a, std::sin(30 * kD2R), 0, std::cos(30 * kD2R));
    a.reset();
    AttitudeOut o = a.update(0, 0, 1);   // first sample after reset is taken as-is
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, o.rollDeg);
}

static void test_axis_map_applied()
{
    Attitude a;
    a.setAxisMap(AxisMap{1, 0, 2, -1, 1, 1});   // ax = -y, ay = x, az = z
    // Sensor y = -sin30 → ax = +sin30 → roll +30
    AttitudeOut o = settle(a, 0, -std::sin(30 * kD2R), std::cos(30 * kD2R));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, o.rollDeg);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_flat_is_level);
    RUN_TEST(test_roll_30);
    RUN_TEST(test_pitch_10_nose_up);
    RUN_TEST(test_vertical_holds_roll);
    RUN_TEST(test_ema_converges_quickly);
    RUN_TEST(test_roll_wrap_does_not_jump);
    RUN_TEST(test_reset);
    RUN_TEST(test_axis_map_applied);
    return UNITY_END();
}
