// core::LoraAirtime: Semtech LoRa time-on-air (S9 data-model §4, research R3).
#include <unity.h>
#include "core/LoraAirtime.h"

using namespace core;

void setUp() {}
void tearDown() {}

static uint32_t air(uint8_t sf, float bw, size_t pl)
{
    return LoraAirtime::airtimeMs(sf, bw, 5, 8, pl, true, true);
}

static void test_default_profile()
{
    TEST_ASSERT_UINT32_WITHIN(2, 185, air(9, 125.0f, 18));   // SF9/BW125/CR4/5, 18 B
}

static void test_sf7()
{
    TEST_ASSERT_UINT32_WITHIN(2, 51, air(7, 125.0f, 18));
}

static void test_sf12_low_data_rate()
{
    // Tsym 32.768 ms ≥ 16 ms → low-data-rate optimisation on: 12.25 + 28 symbols.
    TEST_ASSERT_UINT32_WITHIN(2, 1319, air(12, 125.0f, 18));
}

static void test_bw250_about_half()
{
    const uint32_t a = air(9, 125.0f, 18), b = air(9, 250.0f, 18);
    TEST_ASSERT_UINT32_WITHIN(3, a / 2, b);
}

static void test_monotonic_in_payload()
{
    uint32_t prev = 0;
    for (size_t pl = 1; pl <= 64; pl++) {
        const uint32_t a = air(9, 125.0f, pl);
        TEST_ASSERT_TRUE(a >= prev);
        prev = a;
    }
}

static void test_coding_rate()
{
    TEST_ASSERT_TRUE(LoraAirtime::airtimeMs(9, 125.0f, 8, 8, 18, true, true) > air(9, 125.0f, 18));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_default_profile);
    RUN_TEST(test_sf7);
    RUN_TEST(test_sf12_low_data_rate);
    RUN_TEST(test_bw250_about_half);
    RUN_TEST(test_monotonic_in_payload);
    RUN_TEST(test_coding_rate);
    return UNITY_END();
}
