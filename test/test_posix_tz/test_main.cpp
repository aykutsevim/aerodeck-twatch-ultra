#include <unity.h>
#include "core/ClockMath.h"
#include "core/PosixTz.h"

using core::PosixTz;
using core::utcFromCivil;

void setUp() {}
void tearDown() {}

static int16_t off(const char* tz, int y, int mo, int d, int h, int mi, int s)
{
    PosixTz z;
    TEST_ASSERT_TRUE_MESSAGE(z.parse(tz), tz);
    return z.offsetMinutes(utcFromCivil(y, mo, d, h, mi, s));
}

static void test_fixed_istanbul()
{
    PosixTz z;
    TEST_ASSERT_TRUE(z.parse("<+03>-3"));
    TEST_ASSERT_FALSE(z.hasDst());
    TEST_ASSERT_EQUAL(180, z.stdOffsetMin());
    TEST_ASSERT_EQUAL(180, z.offsetMinutes(utcFromCivil(2027, 7, 1, 12, 0, 0)));
}

static void test_utc() { TEST_ASSERT_EQUAL(0, off("UTC0", 2027, 1, 1, 0, 0, 0)); }

static void test_london_transitions()
{
    const char* tz = "GMT0BST,M3.5.0/1,M10.5.0";
    TEST_ASSERT_EQUAL(0, off(tz, 2027, 3, 28, 0, 59, 59));
    TEST_ASSERT_EQUAL(60, off(tz, 2027, 3, 28, 1, 0, 0));
    TEST_ASSERT_EQUAL(60, off(tz, 2027, 10, 31, 0, 59, 59));
    TEST_ASSERT_EQUAL(0, off(tz, 2027, 10, 31, 1, 0, 0));
    PosixTz z;
    z.parse(tz);
    TEST_ASSERT_TRUE(z.isDst(utcFromCivil(2027, 7, 1, 12, 0, 0)));
    TEST_ASSERT_FALSE(z.isDst(utcFromCivil(2027, 1, 1, 12, 0, 0)));
}

static void test_new_york_transition()
{
    const char* tz = "EST5EDT,M3.2.0,M11.1.0";
    TEST_ASSERT_EQUAL(-300, off(tz, 2027, 3, 14, 6, 59, 59));
    TEST_ASSERT_EQUAL(-240, off(tz, 2027, 3, 14, 7, 0, 0));
    TEST_ASSERT_EQUAL(-240, off(tz, 2027, 11, 7, 5, 59, 59));
    TEST_ASSERT_EQUAL(-300, off(tz, 2027, 11, 7, 6, 0, 0));
}

static void test_southern_hemisphere_sydney()
{
    const char* tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    TEST_ASSERT_EQUAL(660, off(tz, 2027, 1, 15, 0, 0, 0));
    TEST_ASSERT_EQUAL(600, off(tz, 2027, 7, 15, 0, 0, 0));
}

static void test_fractional_offsets()
{
    TEST_ASSERT_EQUAL(330, off("<+0530>-5:30", 2027, 1, 1, 0, 0, 0));
    TEST_ASSERT_EQUAL(345, off("<+0545>-5:45", 2027, 1, 1, 0, 0, 0));
    TEST_ASSERT_EQUAL(-210, off("<-0330>3:30", 2027, 1, 1, 0, 0, 0));
}

static void test_invalid()
{
    PosixTz z;
    TEST_ASSERT_FALSE(z.parse(""));
    TEST_ASSERT_FALSE(z.parse("XYZ"));
    TEST_ASSERT_FALSE(z.parse("GMT0BST,M13.1.0,M10.5.0"));
    TEST_ASSERT_FALSE(z.parse("GMT0BST,M3.5.0"));   // DST without an end rule
    TEST_ASSERT_FALSE(z.parse(nullptr));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fixed_istanbul);
    RUN_TEST(test_utc);
    RUN_TEST(test_london_transitions);
    RUN_TEST(test_new_york_transition);
    RUN_TEST(test_southern_hemisphere_sydney);
    RUN_TEST(test_fractional_offsets);
    RUN_TEST(test_invalid);
    return UNITY_END();
}
