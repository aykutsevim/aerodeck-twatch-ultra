#include <unity.h>
#include <cstring>
#include "core/LogFormat.h"

using core::Lvl;

void setUp() {}
void tearDown() {}

static void test_format_basic()
{
    char buf[64];
    size_t n = core::formatLogLine(buf, sizeof buf, 1234, Lvl::I, "tag", "msg");
    TEST_ASSERT_EQUAL_STRING("[1234][I][tag] msg", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

static void test_format_levels()
{
    char buf[64];
    core::formatLogLine(buf, sizeof buf, 0, Lvl::E, "a", "b");
    TEST_ASSERT_EQUAL_STRING("[0][E][a] b", buf);
    core::formatLogLine(buf, sizeof buf, 7, Lvl::D, "x", "");
    TEST_ASSERT_EQUAL_STRING("[7][D][x] ", buf);
}

static void test_format_truncates_safely()
{
    char buf[10];
    memset(buf, 'Z', sizeof buf);
    core::formatLogLine(buf, sizeof buf, 1234, Lvl::W, "tag", "a long message");
    TEST_ASSERT_EQUAL('\0', buf[9]);
    TEST_ASSERT_EQUAL(9, strlen(buf));
    TEST_ASSERT_EQUAL_STRING_LEN("[1234][W]", buf, 9);
}

static void test_format_zero_size_is_noop()
{
    char c = 'Q';
    TEST_ASSERT_EQUAL(0, core::formatLogLine(&c, 0, 1, Lvl::I, "t", "m"));
    TEST_ASSERT_EQUAL('Q', c);
}

static void test_should_log()
{
    TEST_ASSERT_TRUE(core::shouldLog(Lvl::E, Lvl::E));
    TEST_ASSERT_TRUE(core::shouldLog(Lvl::E, Lvl::D));
    TEST_ASSERT_TRUE(core::shouldLog(Lvl::I, Lvl::I));
    TEST_ASSERT_FALSE(core::shouldLog(Lvl::I, Lvl::W));
    TEST_ASSERT_FALSE(core::shouldLog(Lvl::D, Lvl::I));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_format_basic);
    RUN_TEST(test_format_levels);
    RUN_TEST(test_format_truncates_safely);
    RUN_TEST(test_format_zero_size_is_noop);
    RUN_TEST(test_should_log);
    return UNITY_END();
}
