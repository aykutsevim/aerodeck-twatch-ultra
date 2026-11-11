#include <unity.h>
#include <cstring>
#include "core/ClockMath.h"
#include "core/PosixTz.h"
#include "core/Zones.h"

using namespace core;

void setUp() {}
void tearDown() {}

static int16_t offAt(const Zone& z, int month)
{
    PosixTz t;
    t.parse(z.posix);
    return t.offsetMinutes(utcFromCivil(2027, month, 15, 12, 0, 0));
}

static void test_enough_named_zones()
{
    TEST_ASSERT_TRUE(Zones::namedCount() >= 45);
    TEST_ASSERT_TRUE(Zones::count() > Zones::namedCount());
}

static void test_every_entry_parses_and_names_unique()
{
    for (size_t i = 0; i < Zones::count(); i++) {
        PosixTz t;
        TEST_ASSERT_TRUE_MESSAGE(t.parse(Zones::at(i).posix), Zones::at(i).name);
        for (size_t j = i + 1; j < Zones::count(); j++) {
            TEST_ASSERT_TRUE_MESSAGE(strcmp(Zones::at(i).name, Zones::at(j).name) != 0, Zones::at(i).name);
        }
    }
}

static void test_default_is_istanbul()
{
    const Zone& z = Zones::at(Zones::defaultIndex());
    TEST_ASSERT_EQUAL_STRING("Istanbul", z.name);
    TEST_ASSERT_EQUAL(180, offAt(z, 1));
    TEST_ASSERT_EQUAL(180, offAt(z, 7));
}

static void test_find_case_insensitive()
{
    int i = Zones::find("london");
    TEST_ASSERT_TRUE(i >= 0);
    TEST_ASSERT_EQUAL_STRING("London", Zones::at(size_t(i)).name);
    TEST_ASSERT_EQUAL(-1, Zones::find("Atlantis"));
    TEST_ASSERT_EQUAL(-1, Zones::find(nullptr));
}

static void test_best_match_named()
{
    TEST_ASSERT_EQUAL_STRING("Istanbul", Zones::at(Zones::bestMatch(180)).name);
    TEST_ASSERT_EQUAL_STRING("Kolkata", Zones::at(Zones::bestMatch(330)).name);
    TEST_ASSERT_EQUAL_STRING("Kathmandu", Zones::at(Zones::bestMatch(345)).name);
}

static void test_best_match_every_half_hour_is_exact_all_year()
{
    for (int m = -720; m <= 840; m += 30) {
        const Zone& z = Zones::at(Zones::bestMatch(int16_t(m)));
        TEST_ASSERT_EQUAL_MESSAGE(m, offAt(z, 1), z.name);
        TEST_ASSERT_EQUAL_MESSAGE(m, offAt(z, 7), z.name);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_enough_named_zones);
    RUN_TEST(test_every_entry_parses_and_names_unique);
    RUN_TEST(test_default_is_istanbul);
    RUN_TEST(test_find_case_insensitive);
    RUN_TEST(test_best_match_named);
    RUN_TEST(test_best_match_every_half_hour_is_exact_all_year);
    return UNITY_END();
}
