#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "core/CommandParser.h"

using core::CommandParser;

struct CaptureReply : core::Reply {
    std::vector<std::string> lines;
    void ok() override { lines.push_back("OK"); }
    void err(const char* r) override { lines.push_back(std::string("ERR ") + r); }
    void line(const char* l) override { lines.push_back(l); }
};

static int g_calls;
static int g_argc;
static std::string g_args;

static void handler(int argc, const char* const* argv, core::Reply& r)
{
    g_calls++;
    g_argc = argc;
    g_args.clear();
    for (int i = 0; i < argc; i++) {
        g_args += argv[i];
        g_args += "|";
    }
    r.ok();
}
static void other(int, const char* const*, core::Reply& r) { r.line("other"); }

void setUp() { g_calls = 0; g_argc = -1; g_args.clear(); }
void tearDown() {}

static void test_tokenize()
{
    char line[] = "ad set bright 3";
    const char* argv[8];
    int n = CommandParser::tokenize(line, argv, 8);
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_EQUAL_STRING("ad", argv[0]);
    TEST_ASSERT_EQUAL_STRING("3", argv[3]);
}

static void test_tokenize_collapses_spaces_and_crlf()
{
    char line[] = "  ad   info \r";
    const char* argv[8];
    TEST_ASSERT_EQUAL(2, CommandParser::tokenize(line, argv, 8));
    TEST_ASSERT_EQUAL_STRING("info", argv[1]);
}

static void test_rejects_missing_prefix()
{
    CommandParser p;
    p.add("info", 0, handler);
    CaptureReply r;
    TEST_ASSERT_FALSE(p.dispatch("info", r));
    TEST_ASSERT_EQUAL(0, g_calls);
    TEST_ASSERT_EQUAL_STRING("ERR unknown command", r.lines.back().c_str());
}

static void test_unknown_command()
{
    CommandParser p;
    p.add("info", 0, handler);
    CaptureReply r;
    TEST_ASSERT_FALSE(p.dispatch("ad nope", r));
    TEST_ASSERT_EQUAL_STRING("ERR unknown command", r.lines.back().c_str());
}

static void test_prefix_matching_prefers_longest()
{
    CommandParser p;
    p.add("set", 2, handler);    // ad set <key> <value>
    p.add("set list", 0, other); // ad set list
    p.add("set get", 1, handler);
    CaptureReply r;
    TEST_ASSERT_TRUE(p.dispatch("ad set list", r));
    TEST_ASSERT_EQUAL_STRING("other", r.lines.back().c_str());
    TEST_ASSERT_EQUAL(0, g_calls);

    TEST_ASSERT_TRUE(p.dispatch("ad set get bright", r));
    TEST_ASSERT_EQUAL(1, g_argc);
    TEST_ASSERT_EQUAL_STRING("bright|", g_args.c_str());

    TEST_ASSERT_TRUE(p.dispatch("ad set bright 3", r));
    TEST_ASSERT_EQUAL(2, g_argc);
    TEST_ASSERT_EQUAL_STRING("bright|3|", g_args.c_str());
}

static void test_min_args_enforced()
{
    CommandParser p;
    p.add("set", 2, handler);
    CaptureReply r;
    TEST_ASSERT_FALSE(p.dispatch("ad set bright", r));
    TEST_ASSERT_EQUAL(0, g_calls);
    TEST_ASSERT_EQUAL_STRING("ERR missing arguments", r.lines.back().c_str());
}

static void test_line_too_long()
{
    CommandParser p;
    p.add("info", 0, handler);
    std::string s = "ad info " + std::string(130, 'x');
    CaptureReply r;
    TEST_ASSERT_FALSE(p.dispatch(s.c_str(), r));
    TEST_ASSERT_EQUAL_STRING("ERR line too long", r.lines.back().c_str());
}

static void test_too_many_tokens()
{
    CommandParser p;
    p.add("x", 0, handler);
    CaptureReply r;
    TEST_ASSERT_FALSE(p.dispatch("ad x 1 2 3 4 5 6 7 8 9 10 11", r));   // 13 tokens > 12
    TEST_ASSERT_EQUAL_STRING("ERR too many arguments", r.lines.back().c_str());
}

static void test_simfail_with_all_names_fits()
{
    CommandParser p;
    p.add("simfail", 1, handler);
    CaptureReply r;
    TEST_ASSERT_TRUE(p.dispatch("ad simfail lora nfc gnss sd mic haptics imu rtc", r));
    TEST_ASSERT_EQUAL(8, g_argc);
}

static void test_help_lists_patterns()
{
    CommandParser p;
    p.add("info", 0, handler);
    p.add("set list", 0, other);
    TEST_ASSERT_EQUAL(2, p.count());
    TEST_ASSERT_EQUAL_STRING("set list", p.pattern(1));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tokenize);
    RUN_TEST(test_tokenize_collapses_spaces_and_crlf);
    RUN_TEST(test_rejects_missing_prefix);
    RUN_TEST(test_unknown_command);
    RUN_TEST(test_prefix_matching_prefers_longest);
    RUN_TEST(test_min_args_enforced);
    RUN_TEST(test_line_too_long);
    RUN_TEST(test_too_many_tokens);
    RUN_TEST(test_simfail_with_all_names_fits);
    RUN_TEST(test_help_lists_patterns);
    return UNITY_END();
}
