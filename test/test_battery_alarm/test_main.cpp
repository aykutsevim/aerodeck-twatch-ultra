// core::BatteryAlarm: STALL < 15 %, PULL UP <= 5 %, hysteresis 17 % / 7 %, charging clears (S4 data-model §3).
#include <unity.h>
#include <cstdio>
#include <string>
#include "core/BatteryAlarm.h"

using namespace core;
using A = BattAction;

void setUp() {}
void tearDown() {}

static void test_stall_and_hysteresis()
{
    BatteryAlarm b;
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(16, false, true)));
    TEST_ASSERT_EQUAL(int(A::RaiseStall), int(b.update(14, false, true)));
    TEST_ASSERT_EQUAL(int(BattState::Stall), int(b.state()));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(15, false, true)));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(16, false, true)));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(14, false, true)));
    TEST_ASSERT_EQUAL(int(A::ClearAll), int(b.update(17, false, true)));
    TEST_ASSERT_EQUAL(int(BattState::Normal), int(b.state()));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(15, false, true)));   // 15 is not < 15
}

static void test_pullup_and_deescalation()
{
    BatteryAlarm b;
    b.update(14, false, true);
    TEST_ASSERT_EQUAL(int(A::RaisePullUp), int(b.update(5, false, true)));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(6, false, true)));
    TEST_ASSERT_EQUAL(int(A::RaiseStallAcked), int(b.update(7, false, true)));
    TEST_ASSERT_EQUAL(int(BattState::Stall), int(b.state()));
    TEST_ASSERT_EQUAL(int(A::ClearAll), int(b.update(17, false, true)));
}

static void test_direct_pullup_and_jump_to_normal()
{
    BatteryAlarm b;
    TEST_ASSERT_EQUAL(int(A::RaisePullUp), int(b.update(3, false, true)));
    TEST_ASSERT_EQUAL(int(A::ClearAll), int(b.update(20, false, true)));   // PullUp straight to Normal
}

static void test_charging_and_invalid()
{
    BatteryAlarm b;
    b.update(3, false, true);
    TEST_ASSERT_EQUAL(int(A::ClearAll), int(b.update(3, true, true)));
    TEST_ASSERT_EQUAL(int(BattState::Normal), int(b.state()));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(3, true, true)));    // stays Normal while charging
    TEST_ASSERT_EQUAL(int(A::RaisePullUp), int(b.update(3, false, true)));
    TEST_ASSERT_EQUAL(int(A::None), int(b.update(50, false, false)));  // invalid reading ignored
    TEST_ASSERT_EQUAL(int(BattState::PullUp), int(b.state()));
    BatteryAlarm c;
    TEST_ASSERT_EQUAL(int(A::None), int(c.update(0, false, false)));
}

static void test_sweep_sc001()
{
    BatteryAlarm b;
    std::string log;
    for (int p = 20; p >= 3; p--) {
        A a = b.update(p, false, true);
        if (a != A::None) log += std::to_string(int(a)) + "@" + std::to_string(p) + " ";
    }
    for (int p = 3; p <= 20; p++) {
        A a = b.update(p, false, true);
        if (a != A::None) log += std::to_string(int(a)) + "@" + std::to_string(p) + " ";
    }
    char want[96];
    snprintf(want, sizeof want, "%d@14 %d@5 %d@7 %d@17 ", int(A::RaiseStall), int(A::RaisePullUp),
             int(A::RaiseStallAcked), int(A::ClearAll));
    TEST_ASSERT_EQUAL_STRING(want, log.c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_stall_and_hysteresis);
    RUN_TEST(test_pullup_and_deescalation);
    RUN_TEST(test_direct_pullup_and_jump_to_normal);
    RUN_TEST(test_charging_and_invalid);
    RUN_TEST(test_sweep_sc001);
    return UNITY_END();
}
