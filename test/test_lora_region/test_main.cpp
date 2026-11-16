// core::LoraRegion: legal radio limits per region (S9 data-model §2, research R1, FR-002).
#include <unity.h>
#include "core/LoraAirtime.h"
#include "core/LoraRegion.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_eu868_default()
{
    uint8_t cl = 0xFF;
    const LoraProfile p = LoraRegion::resolve(0, 10, 9, 0, cl);
    TEST_ASSERT_EQUAL(0, cl);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 869.525f, p.freqMHz);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, p.bwKHz);
    TEST_ASSERT_EQUAL(9, p.sf);
    TEST_ASSERT_EQUAL(5, p.crDenom);
    TEST_ASSERT_EQUAL(10, p.powerDbm);
    TEST_ASSERT_EQUAL_HEX8(0x12, p.syncWord);
    TEST_ASSERT_EQUAL(8, p.preamble);
    const RegionLimits& r = LoraRegion::limits(0);
    TEST_ASSERT_EQUAL_STRING("EU868", r.name);
    TEST_ASSERT_TRUE(p.freqMHz >= r.freqMinMHz && p.freqMHz <= r.freqMaxMHz);
    TEST_ASSERT_EQUAL(10, r.dutyPerMille);
    TEST_ASSERT_EQUAL(0, r.maxDwellMs);
}

static void test_power_clamped()
{
    uint8_t cl = 0;
    TEST_ASSERT_EQUAL(14, LoraRegion::resolve(0, 20, 9, 0, cl).powerDbm);
    TEST_ASSERT_TRUE(cl & LoraRegion::kClampPower);
    TEST_ASSERT_EQUAL(2, LoraRegion::resolve(0, 0, 9, 0, cl).powerDbm);
    TEST_ASSERT_TRUE(cl & LoraRegion::kClampPower);
    TEST_ASSERT_EQUAL(20, LoraRegion::resolve(1, 20, 9, 0, cl).powerDbm);
    TEST_ASSERT_EQUAL(0, cl);
}

static void test_sf_and_bw_clamped()
{
    uint8_t cl = 0;
    TEST_ASSERT_EQUAL(12, LoraRegion::resolve(0, 10, 13, 0, cl).sf);
    TEST_ASSERT_EQUAL(LoraRegion::kClampSf, cl);
    TEST_ASSERT_EQUAL(7, LoraRegion::resolve(0, 10, 5, 0, cl).sf);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 250.0f, LoraRegion::resolve(0, 10, 9, 1, cl).bwKHz);
    TEST_ASSERT_EQUAL(0, cl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, LoraRegion::resolve(0, 10, 9, 2, cl).bwKHz);   // EU868 has 2 BWs
    TEST_ASSERT_EQUAL(LoraRegion::kClampBw, cl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, LoraRegion::resolve(1, 10, 9, 2, cl).bwKHz);
    TEST_ASSERT_EQUAL(0, cl);
}

static void test_bad_region()
{
    uint8_t cl = 0;
    const LoraProfile p = LoraRegion::resolve(7, 10, 9, 0, cl);
    TEST_ASSERT_TRUE(cl & LoraRegion::kClampRegion);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 869.525f, p.freqMHz);   // falls back to EU868
}

static void test_us915_dwell()
{
    uint8_t cl = 0;
    const LoraProfile slow = LoraRegion::resolve(1, 10, 12, 0, cl);
    const uint32_t a = LoraAirtime::airtimeMs(slow.sf, slow.bwKHz, slow.crDenom, slow.preamble, 18, true, true);
    TEST_ASSERT_EQUAL(400, LoraRegion::limits(1).maxDwellMs);
    TEST_ASSERT_FALSE(LoraRegion::dwellOk(1, a));
    TEST_ASSERT_TRUE(LoraRegion::dwellOk(1, 185));
    TEST_ASSERT_TRUE(LoraRegion::dwellOk(0, a));   // EU868 has no dwell limit
    const LoraProfile p = LoraRegion::resolve(1, 10, 9, 0, cl);
    TEST_ASSERT_TRUE(p.freqMHz >= 902.3f && p.freqMHz <= 914.9f);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_eu868_default);
    RUN_TEST(test_power_clamped);
    RUN_TEST(test_sf_and_bw_clamped);
    RUN_TEST(test_bad_region);
    RUN_TEST(test_us915_dwell);
    return UNITY_END();
}
