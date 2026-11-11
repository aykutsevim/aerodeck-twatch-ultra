#include <unity.h>
#include <cmath>
#include "core/Fuel.h"

using core::FuelFlow;
using core::FuelState;

void setUp() {}
void tearDown() {}

static void test_nan_before_10_minutes()
{
    FuelFlow f;
    for (uint32_t m = 0; m < 10; m++) {
        f.addSample(m, uint8_t(90 - m / 10), false);
    }
    TEST_ASSERT_TRUE(std::isnan(f.ratePctPerH()));   // span 9 min
    f.addSample(10, 89, false);
    TEST_ASSERT_FALSE(std::isnan(f.ratePctPerH()));   // span 10 min
}

static void test_constant_drain_6_per_hour()
{
    FuelFlow f;
    // 1 % every 10 minutes → 6 %/h. Integer % values like the PMU gives.
    for (uint32_t m = 0; m <= 30; m++) {
        f.addSample(m, uint8_t(90 - m / 10), false);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 6.0f, f.ratePctPerH());
}

static void test_noise_tolerated()
{
    FuelFlow f;
    const int noise[] = {0, 1, -1, 0, 1, 0, -1, 1, 0, -1};
    for (uint32_t m = 0; m < 30; m++) {
        int pct = 80 - int(m) / 10 + noise[m % 10];
        f.addSample(m, uint8_t(pct), false);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 6.0f, f.ratePctPerH());
}

static void test_charging_resets()
{
    FuelFlow f;
    for (uint32_t m = 0; m <= 20; m++) {
        f.addSample(m, uint8_t(90 - m / 10), false);
    }
    TEST_ASSERT_FALSE(std::isnan(f.ratePctPerH()));
    f.addSample(21, 88, true);   // charging: history cleared, no rate
    TEST_ASSERT_TRUE(std::isnan(f.ratePctPerH()));
    f.addSample(22, 88, false);   // back on battery: starts fresh
    TEST_ASSERT_TRUE(std::isnan(f.ratePctPerH()));
}

static void test_jump_resets()
{
    FuelFlow f;
    for (uint32_t m = 0; m <= 20; m++) {
        f.addSample(m, 80, false);
    }
    f.addSample(21, 70, false);   // >5 % jump (gauge recalibration)
    TEST_ASSERT_TRUE(std::isnan(f.ratePctPerH()));
    TEST_ASSERT_EQUAL(1, f.count());
}

static void test_ring_keeps_last_30()
{
    FuelFlow f;
    for (uint32_t m = 0; m < 100; m++) {
        f.addSample(m, uint8_t(100 - m / 10), false);
    }
    TEST_ASSERT_EQUAL(30, f.count());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 6.0f, f.ratePctPerH());
}

static void test_low_fuel_hysteresis()
{
    FuelState s;
    TEST_ASSERT_FALSE(s.update(15, false));
    TEST_ASSERT_TRUE(s.update(14, false));
    TEST_ASSERT_TRUE(s.update(16, false));
    TEST_ASSERT_TRUE(s.update(17, false));
    TEST_ASSERT_FALSE(s.update(18, false));
    TEST_ASSERT_TRUE(s.update(10, false));
    TEST_ASSERT_FALSE(s.update(10, true));   // charging clears immediately
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nan_before_10_minutes);
    RUN_TEST(test_constant_drain_6_per_hour);
    RUN_TEST(test_noise_tolerated);
    RUN_TEST(test_charging_resets);
    RUN_TEST(test_jump_resets);
    RUN_TEST(test_ring_keeps_last_30);
    RUN_TEST(test_low_fuel_hysteresis);
    return UNITY_END();
}
