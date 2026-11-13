// core::EjectGesture: double tap + pull (S7 data-model §4, FR-007, NFR-USE-003). Travel 171 px → fire at 68.4 px.
#include <unity.h>
#include "core/EjectGesture.h"

using namespace core;
using S = EjectGesture::State;
using C = EjectGesture::Cancel;

void setUp() {}
void tearDown() {}

static void tap(EjectGesture& g, uint32_t& t, uint32_t holdMs = 80, int32_t x = 200, int32_t y = 20)
{
    g.press(x, y, t);
    t += holdMs;
    g.release(x, y, t);
}

static void pull(EjectGesture& g, uint32_t& t, int32_t dy, int32_t dx = 0)
{
    g.press(200, 20, t);
    for (int32_t s = 1; s <= 10; s++) {
        t += 20;
        g.move(200 + dx * s / 10, 20 + dy * s / 10, t);
    }
    t += 20;
    g.release(200 + dx, 20 + dy, t);
}

static void arm(EjectGesture& g, uint32_t& t)
{
    tap(g, t);
    t += 150;
    tap(g, t);
}

static void test_full_gesture_fires()
{
    EjectGesture g;
    uint32_t t = 1000;
    arm(g, t);
    TEST_ASSERT_EQUAL(int(S::Armed), int(g.state()));
    t += 300;
    g.press(200, 20, t);
    g.move(200, 60, t + 50);
    TEST_ASSERT_EQUAL(int(S::Pulling), int(g.state()));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f / 171.0f, g.pullFrac());
    g.move(200, 500, t + 100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, g.pullFrac());   // clamped
    g.move(200, 90, t + 150);
    g.release(200, 90, t + 200);   // 70 px ≥ 68.4
    TEST_ASSERT_TRUE(g.fired());
}

static void test_single_tap()
{
    EjectGesture g;
    uint32_t t = 0;
    tap(g, t);
    t += 1000;
    g.tick(t);
    pull(g, t, 120);
    TEST_ASSERT_FALSE(g.fired());
}

static void test_drag_without_taps()
{
    EjectGesture g;
    uint32_t t = 0;
    pull(g, t, 150);
    TEST_ASSERT_FALSE(g.fired());
    TEST_ASSERT_EQUAL(int(S::Idle), int(g.state()));
}

static void test_slow_double_tap()
{
    EjectGesture g;
    uint32_t t = 0;
    tap(g, t);
    t += 450;   // gap > 400 ms
    tap(g, t);
    TEST_ASSERT_EQUAL(int(S::FirstTap), int(g.state()));   // the second tap is a new first tap
    t += 100;
    pull(g, t, 120);
    TEST_ASSERT_FALSE(g.fired());
}

static void test_pull_after_armed_window()
{
    EjectGesture g;
    uint32_t t = 0;
    arm(g, t);
    t += 3100;
    pull(g, t, 120);
    TEST_ASSERT_FALSE(g.fired());
}

static void test_timeout_reason()
{
    EjectGesture g;
    uint32_t t = 0;
    arm(g, t);
    g.tick(t + 3001);
    TEST_ASSERT_EQUAL(int(S::Idle), int(g.state()));
    TEST_ASSERT_EQUAL(int(C::Timeout), int(g.takeCancel()));
    TEST_ASSERT_EQUAL(int(C::None), int(g.takeCancel()));
}

static void test_sideways_pull()
{
    EjectGesture g;
    uint32_t t = 0;
    arm(g, t);
    t += 100;
    pull(g, t, 100, 30);   // 30 px sideways > 20
    TEST_ASSERT_FALSE(g.fired());
    TEST_ASSERT_EQUAL(int(C::Side), int(g.takeCancel()));
}

static void test_short_pull()
{
    EjectGesture g;
    uint32_t t = 0;
    arm(g, t);
    t += 100;
    pull(g, t, 60);   // 35 %
    TEST_ASSERT_FALSE(g.fired());
    TEST_ASSERT_EQUAL(int(S::Idle), int(g.state()));
    TEST_ASSERT_EQUAL(int(C::Short), int(g.takeCancel()));
    // a new double tap re-arms
    t += 500;
    arm(g, t);
    TEST_ASSERT_EQUAL(int(S::Armed), int(g.state()));
    t += 100;
    pull(g, t, 80);
    TEST_ASSERT_TRUE(g.fired());
}

static void test_long_press_is_not_a_tap()
{
    EjectGesture g;
    uint32_t t = 0;
    tap(g, t, 350);   // > 300 ms
    t += 100;
    tap(g, t);
    TEST_ASSERT_EQUAL(int(S::FirstTap), int(g.state()));   // only one valid tap so far
    t += 100;
    pull(g, t, 120);
    TEST_ASSERT_FALSE(g.fired());
}

static void test_moving_tap_is_not_a_tap()
{
    EjectGesture g;
    uint32_t t = 0;
    tap(g, t);
    t += 100;
    g.press(200, 20, t);
    g.move(215, 20, t + 30);   // 15 px > 10
    g.release(215, 20, t + 60);
    t += 60;
    TEST_ASSERT_NOT_EQUAL(int(S::Armed), int(g.state()));
    t += 100;
    pull(g, t, 120);
    TEST_ASSERT_FALSE(g.fired());
}

static void test_fired_is_terminal_until_reset()
{
    EjectGesture g;
    uint32_t t = 0;
    arm(g, t);
    t += 100;
    pull(g, t, 100);
    TEST_ASSERT_TRUE(g.fired());
    g.tick(t + 10000);
    tap(g, t);
    TEST_ASSERT_TRUE(g.fired());
    g.reset();
    TEST_ASSERT_EQUAL(int(S::Idle), int(g.state()));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_full_gesture_fires);
    RUN_TEST(test_single_tap);
    RUN_TEST(test_drag_without_taps);
    RUN_TEST(test_slow_double_tap);
    RUN_TEST(test_pull_after_armed_window);
    RUN_TEST(test_timeout_reason);
    RUN_TEST(test_sideways_pull);
    RUN_TEST(test_short_pull);
    RUN_TEST(test_long_press_is_not_a_tap);
    RUN_TEST(test_moving_tap_is_not_a_tap);
    RUN_TEST(test_fired_is_terminal_until_reset);
    return UNITY_END();
}
