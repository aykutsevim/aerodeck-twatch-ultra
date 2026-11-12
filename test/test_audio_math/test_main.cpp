// core::AudioMath volume steps and gain (S5 data-model §3, FR-011).
#include <unity.h>
#include "core/AudioMath.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_gain_steps()
{
    const int16_t expect[5] = {0, 9830, 16384, 23170, 32767};
    for (uint8_t i = 0; i < 5; i++) TEST_ASSERT_EQUAL_INT16(expect[i], AudioMath::gainQ15(i));
    TEST_ASSERT_EQUAL_INT16(0, AudioMath::gainQ15(5));
    TEST_ASSERT_EQUAL_INT16(0, AudioMath::gainQ15(255));
}

static void test_monotonic()
{
    for (uint8_t i = 1; i < 5; i++) TEST_ASSERT_TRUE(AudioMath::gainQ15(i) > AudioMath::gainQ15(uint8_t(i - 1)));
}

static void test_apply_gain()
{
    int16_t s[6] = {32767, -32768, 1000, -1000, 0, 16384};
    AudioMath::applyGain(s, 6, 32767);   // full: unchanged
    TEST_ASSERT_EQUAL_INT16(32767, s[0]);
    TEST_ASSERT_EQUAL_INT16(-32768, s[1]);
    TEST_ASSERT_EQUAL_INT16(1000, s[2]);

    int16_t h[4] = {32767, -32768, 1000, -1000};
    AudioMath::applyGain(h, 4, 16384);   // half
    TEST_ASSERT_INT16_WITHIN(1, 16383, h[0]);
    TEST_ASSERT_EQUAL_INT16(-16384, h[1]);
    TEST_ASSERT_EQUAL_INT16(500, h[2]);
    TEST_ASSERT_EQUAL_INT16(-500, h[3]);

    int16_t z[3] = {32767, -32768, 123};
    AudioMath::applyGain(z, 3, 0);
    TEST_ASSERT_EQUAL_INT16(0, z[0]);
    TEST_ASSERT_EQUAL_INT16(0, z[1]);
    TEST_ASSERT_EQUAL_INT16(0, z[2]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_gain_steps);
    RUN_TEST(test_monotonic);
    RUN_TEST(test_apply_gain);
    return UNITY_END();
}
