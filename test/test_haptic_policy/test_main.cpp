// core::HapticPolicy: pattern x intensity x quiet → effect sequence, and priority (S4 data-model §4, research R2).
#include <unity.h>
#include <initializer_list>
#include "core/HapticPolicy.h"

using namespace core;

void setUp() {}
void tearDown() {}

static bool sel(Pattern p, Intensity i, bool q, Seq& s) { return HapticPolicy::select(p, i, q, s); }

static void test_click()
{
    Seq s;
    TEST_ASSERT_FALSE(sel(Pattern::Click, Intensity::Off, false, s));
    TEST_ASSERT_TRUE(sel(Pattern::Click, Intensity::Low, false, s));
    TEST_ASSERT_EQUAL(1, s.n);
    TEST_ASSERT_EQUAL(5, s.w[0]);   // Sharp Click 60 %
    TEST_ASSERT_TRUE(sel(Pattern::Click, Intensity::High, false, s));
    TEST_ASSERT_EQUAL(4, s.w[0]);   // Sharp Click 100 %
    TEST_ASSERT_TRUE(sel(Pattern::Click, Intensity::High, true, s));   // quiet doesn't affect clicks
    TEST_ASSERT_EQUAL(1, s.prio);
}

static void test_caution_advisory()
{
    Seq s;
    for (Pattern p : {Pattern::Caution, Pattern::Advisory}) {
        TEST_ASSERT_FALSE(sel(p, Intensity::Off, false, s));
        TEST_ASSERT_FALSE(sel(p, Intensity::Low, true, s));
        TEST_ASSERT_FALSE(sel(p, Intensity::High, true, s));
        TEST_ASSERT_TRUE(sel(p, Intensity::Low, false, s));
        TEST_ASSERT_TRUE(sel(p, Intensity::High, false, s));
    }
    sel(Pattern::Caution, Intensity::High, false, s);
    TEST_ASSERT_EQUAL(12, s.w[0]);
    TEST_ASSERT_EQUAL(3, s.prio);
    sel(Pattern::Caution, Intensity::Low, false, s);
    TEST_ASSERT_EQUAL(11, s.w[0]);
    sel(Pattern::Advisory, Intensity::High, false, s);
    TEST_ASSERT_EQUAL(7, s.w[0]);
    TEST_ASSERT_EQUAL(2, s.prio);
    sel(Pattern::Advisory, Intensity::Low, false, s);
    TEST_ASSERT_EQUAL(8, s.w[0]);
}

static void test_stick_shaker_always_full()
{
    Seq ref, s;
    TEST_ASSERT_TRUE(sel(Pattern::StickShaker, Intensity::High, false, ref));
    TEST_ASSERT_EQUAL(4, ref.prio);
    TEST_ASSERT_TRUE(ref.n <= 8);
    int pulses = 0, waits = 0;
    for (int i = 0; i < ref.n; i++) {
        if (ref.w[i] == 52) pulses++;
        if (ref.w[i] == (0x80 | 10)) waits++;
    }
    TEST_ASSERT_EQUAL(3, pulses);
    TEST_ASSERT_EQUAL(2, waits);
    TEST_ASSERT_TRUE(ref.lengthMs >= 1000 && ref.lengthMs <= 2000);
    for (Intensity i : {Intensity::Off, Intensity::Low, Intensity::High}) {
        for (bool q : {false, true}) {
            TEST_ASSERT_TRUE(sel(Pattern::StickShaker, i, q, s));
            TEST_ASSERT_EQUAL(ref.n, s.n);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(ref.w, s.w, ref.n);
        }
    }
    TEST_ASSERT_FALSE(sel(Pattern::None, Intensity::High, false, s));
}

static void test_should_start()
{
    // a click (1) during a running shaker (4) → dropped
    TEST_ASSERT_FALSE(HapticPolicy::shouldStart(1, 4, 2000, 1000));
    // after the shaker ended → plays
    TEST_ASSERT_TRUE(HapticPolicy::shouldStart(1, 4, 2000, 2000));
    // equal or higher priority replaces
    TEST_ASSERT_TRUE(HapticPolicy::shouldStart(4, 4, 2000, 1000));
    TEST_ASSERT_TRUE(HapticPolicy::shouldStart(3, 1, 2000, 1000));
    // millis() wrap-around
    TEST_ASSERT_FALSE(HapticPolicy::shouldStart(1, 4, 100u, 0xFFFFFF00u));
    TEST_ASSERT_TRUE(HapticPolicy::shouldStart(1, 4, 0xFFFFFF00u, 100u));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_click);
    RUN_TEST(test_caution_advisory);
    RUN_TEST(test_stick_shaker_always_full);
    RUN_TEST(test_should_start);
    return UNITY_END();
}
