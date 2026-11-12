// core::StepAccumulator: hardware step count → deltas (S6 data-model §3, FR-010, edge case "implausible jumps").
#include <unity.h>
#include "core/StepAccumulator.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_counting()
{
    StepAccumulator a;
    a.reset(1000);   // the hub was just started: its count is 0
    bool rej = true;
    TEST_ASSERT_EQUAL_UINT32(3, a.feed(3, 2000, rej));
    TEST_ASSERT_FALSE(rej);
    TEST_ASSERT_EQUAL_UINT32(2, a.feed(5, 3000, rej));
    TEST_ASSERT_EQUAL_UINT32(0, a.feed(5, 4000, rej));
    TEST_ASSERT_EQUAL_UINT32(5, a.last());
}

static void test_sensor_restart()
{
    StepAccumulator a;
    a.reset(0);
    bool rej;
    a.feed(40, 10000, rej);
    TEST_ASSERT_EQUAL_UINT32(4, a.feed(4, 11000, rej));   // the hub restarted from 0 and counted 4
    TEST_ASSERT_FALSE(rej);
    TEST_ASSERT_EQUAL_UINT32(1, a.feed(5, 12000, rej));
}

static void test_implausible_jump()
{
    StepAccumulator a;
    a.reset(0);
    bool rej;
    a.feed(10, 1000, rej);
    // 1 s later: allowed 6 x 1 + 20 = 26; 5000 is not plausible
    TEST_ASSERT_EQUAL_UINT32(0, a.feed(5010, 2000, rej));
    TEST_ASSERT_TRUE(rej);
    TEST_ASSERT_EQUAL_UINT32(5010, a.last());   // re-baselined
    TEST_ASSERT_EQUAL_UINT32(2, a.feed(5012, 3000, rej));
    TEST_ASSERT_FALSE(rej);
    // 60 s gap: 6 x 60 + 20 = 380 is allowed
    TEST_ASSERT_EQUAL_UINT32(380, a.feed(5392, 63000, rej));
    TEST_ASSERT_FALSE(rej);
}

static void test_millis_wrap()
{
    StepAccumulator a;
    a.reset(0xFFFFF000u);
    bool rej;
    TEST_ASSERT_EQUAL_UINT32(20, a.feed(20, 0x00001000u, rej));   // 8.2 s across the wrap
    TEST_ASSERT_FALSE(rej);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_counting);
    RUN_TEST(test_sensor_restart);
    RUN_TEST(test_implausible_jump);
    RUN_TEST(test_millis_wrap);
    return UNITY_END();
}
