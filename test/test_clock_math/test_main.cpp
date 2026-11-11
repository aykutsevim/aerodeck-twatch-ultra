#include <unity.h>
#include "core/ClockMath.h"

using namespace core;

void setUp() {}
void tearDown() {}

// 2026-11-11T14:37:20Z
static const int64_t T0 = 1794407840;

static void test_parse_reference()
{
    int64_t t = 0;
    TEST_ASSERT_TRUE(parseIso8601Utc("2026-11-11T14:37:20Z", t));
    TEST_ASSERT_EQUAL_INT64(T0, t);
    TEST_ASSERT_TRUE(parseIso8601Utc("1970-01-01T00:00:00Z", t));
    TEST_ASSERT_EQUAL_INT64(0, t);
}

static void test_split_utc()
{
    LocalTime l = splitLocal(T0, 0, 0);
    TEST_ASSERT_EQUAL(14, l.h);
    TEST_ASSERT_EQUAL(37, l.m);
    TEST_ASSERT_EQUAL(20, l.s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, l.headingDeg);
}

static void test_heading_fraction()
{
    LocalTime l = splitLocal(T0, 500, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 123.0f, l.headingDeg);
}

static void test_heading_never_reaches_360()
{
    LocalTime l = splitLocal(T0 + 39, 999, 0);   // :59.999
    TEST_ASSERT_EQUAL(59, l.s);
    TEST_ASSERT_TRUE(l.headingDeg < 360.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 359.994f, l.headingDeg);
}

static void test_tz_crosses_midnight()
{
    int64_t t = 0;
    TEST_ASSERT_TRUE(parseIso8601Utc("2026-11-11T21:00:00Z", t));
    LocalTime l = splitLocal(t, 0, 180);
    TEST_ASSERT_EQUAL(0, l.h);
    TEST_ASSERT_EQUAL(0, l.m);
}

static void test_tz_negative()
{
    LocalTime l = splitLocal(T0, 0, -300);   // UTC-5 → 09:37
    TEST_ASSERT_EQUAL(9, l.h);
    TEST_ASSERT_EQUAL(37, l.m);
}

static void test_tz_half_hour()
{
    LocalTime l = splitLocal(T0, 0, 330);   // UTC+5:30 → 20:07
    TEST_ASSERT_EQUAL(20, l.h);
    TEST_ASSERT_EQUAL(7, l.m);
}

static void test_tz_negative_before_epoch_day()
{
    LocalTime l = splitLocal(3600, 0, -300);   // 1970-01-01T01:00Z → 20:00 previous day
    TEST_ASSERT_EQUAL(20, l.h);
}

static void test_parse_rejects_bad()
{
    int64_t t = 0;
    TEST_ASSERT_FALSE(parseIso8601Utc("2026-11-11T14:37:20", t));    // missing Z
    TEST_ASSERT_FALSE(parseIso8601Utc("2026-13-11T14:37:20Z", t));   // month 13
    TEST_ASSERT_FALSE(parseIso8601Utc("2026-02-30T00:00:00Z", t));   // Feb 30
    TEST_ASSERT_FALSE(parseIso8601Utc("2026-11-11T24:00:00Z", t));   // hour 24
    TEST_ASSERT_FALSE(parseIso8601Utc("garbage", t));
    TEST_ASSERT_FALSE(parseIso8601Utc(nullptr, t));
}

static void test_parse_leap_day()
{
    int64_t t = 0;
    TEST_ASSERT_TRUE(parseIso8601Utc("2028-02-29T12:00:00Z", t));
    TEST_ASSERT_FALSE(parseIso8601Utc("2027-02-29T12:00:00Z", t));
}

static void test_civil_roundtrip()
{
    int y, mo, d, h, mi, s;
    civilFromUtc(T0, y, mo, d, h, mi, s);
    TEST_ASSERT_EQUAL(2026, y);
    TEST_ASSERT_EQUAL(11, mo);
    TEST_ASSERT_EQUAL(11, d);
    TEST_ASSERT_EQUAL(14, h);
    TEST_ASSERT_EQUAL(37, mi);
    TEST_ASSERT_EQUAL(20, s);
    TEST_ASSERT_EQUAL_INT64(T0, utcFromCivil(2026, 11, 11, 14, 37, 20));
}

static void test_plausible()
{
    TEST_ASSERT_FALSE(clockPlausible(2024));
    TEST_ASSERT_TRUE(clockPlausible(2025));
    TEST_ASSERT_TRUE(clockPlausible(2026));
    TEST_ASSERT_FALSE(clockPlausible(2200));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_reference);
    RUN_TEST(test_split_utc);
    RUN_TEST(test_heading_fraction);
    RUN_TEST(test_heading_never_reaches_360);
    RUN_TEST(test_tz_crosses_midnight);
    RUN_TEST(test_tz_negative);
    RUN_TEST(test_tz_half_hour);
    RUN_TEST(test_tz_negative_before_epoch_day);
    RUN_TEST(test_parse_rejects_bad);
    RUN_TEST(test_parse_leap_day);
    RUN_TEST(test_civil_roundtrip);
    RUN_TEST(test_plausible);
    return UNITY_END();
}
