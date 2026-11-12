// core::FlightMath: NM, centi-g and display formats (S6 data-model §4, FR-001, FR-003, FR-005).
#include <unity.h>
#include "core/FlightMath.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_nm()
{
    // Unity double support is disabled in this build: compare as float
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.000270f, float(FlightMath::nm(2470, 75)));   // 1852.5 m
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, float(FlightMath::nm(0, 75)));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, float(FlightMath::nm(1852, 100)));
}

static void test_format_nm()
{
    char b[16];
    FlightMath::formatNm(1.5, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
    FlightMath::formatNm(0.0, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("0.00", b);
    FlightMath::formatNm(1.00027, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("1.00", b);
    FlightMath::formatNm(9.99, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("9.99", b);
    FlightMath::formatNm(9.996, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("10.0", b);   // would round to 10.00: switch to one decimal
    FlightMath::formatNm(10.04, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("10.0", b);
    FlightMath::formatNm(123.44, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("123.4", b);
}

static void test_centi_g()
{
    bool sat = true;
    TEST_ASSERT_EQUAL_UINT16(100, FlightMath::toCentiG(1.0f, 16.0f, sat));
    TEST_ASSERT_FALSE(sat);
    TEST_ASSERT_EQUAL_UINT16(271, FlightMath::toCentiG(2.714f, 16.0f, sat));
    TEST_ASSERT_EQUAL_UINT16(1600, FlightMath::toCentiG(16.0f, 16.0f, sat));
    TEST_ASSERT_TRUE(sat);
    TEST_ASSERT_EQUAL_UINT16(1600, FlightMath::toCentiG(22.0f, 16.0f, sat));
    TEST_ASSERT_TRUE(sat);
    TEST_ASSERT_EQUAL_UINT16(0, FlightMath::toCentiG(-1.0f, 16.0f, sat));
    TEST_ASSERT_FALSE(sat);
    TEST_ASSERT_EQUAL_UINT16(2000, FlightMath::toCentiG(20.0f, 0.0f, sat));   // unknown range: no clamp
    TEST_ASSERT_FALSE(sat);
}

static void test_format_g()
{
    char b[16];
    FlightMath::formatG(270, false, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
    FlightMath::formatG(100, true, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("1.0", b);
    FlightMath::formatG(271, true, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("2.7", b);
    FlightMath::formatG(275, true, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("2.8", b);
    FlightMath::formatG(1600, true, true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("16.0+", b);
    FlightMath::formatG(0, true, false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("0.0", b);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nm);
    RUN_TEST(test_format_nm);
    RUN_TEST(test_centi_g);
    RUN_TEST(test_format_g);
    return UNITY_END();
}
