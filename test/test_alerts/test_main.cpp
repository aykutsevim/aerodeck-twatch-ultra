// core::AlertSet + core::Alerts table (S4 data-model §1–2, spec FR-001..FR-003, FR-008, SC-002).
#include <unity.h>
#include <cstdlib>
#include <cstring>
#include "core/AlertSet.h"
#include "core/Alerts.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_order_level_then_time()
{
    AlertSet s;
    s.raise(AlertId::TimeUnsync, AlertLevel::Advisory, "TIME UNSYNC");
    s.raise(AlertId::ClockNotSet, AlertLevel::Caution, "CLOCK NOT SET");
    s.raise(AlertId::SafeMode, AlertLevel::Caution, "SAFE MODE");
    s.raise(AlertId::Stall, AlertLevel::Warning, "STALL");
    Alert out[8];
    TEST_ASSERT_EQUAL(4, int(s.sorted(out, 8)));
    TEST_ASSERT_EQUAL(int(AlertId::Stall), int(out[0].id));
    TEST_ASSERT_EQUAL(int(AlertId::ClockNotSet), int(out[1].id));   // same level: raised first is first
    TEST_ASSERT_EQUAL(int(AlertId::SafeMode), int(out[2].id));
    TEST_ASSERT_EQUAL(int(AlertId::TimeUnsync), int(out[3].id));
}

static void test_random_orders_warning_first()
{
    const AlertId ids[5] = {AlertId::Test1, AlertId::Test2, AlertId::ClockNotSet, AlertId::Stall, AlertId::Test3};
    const AlertLevel lv[5] = {AlertLevel::Advisory, AlertLevel::Caution, AlertLevel::Caution, AlertLevel::Warning,
                              AlertLevel::Caution};
    srand(7);
    for (int run = 0; run < 10; run++) {
        int order[5] = {0, 1, 2, 3, 4};
        for (int i = 4; i > 0; i--) {
            int j = rand() % (i + 1);
            int t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        AlertSet s;
        for (int k : order) {
            s.raise(ids[k], lv[k], "X");
        }
        Alert out[5];
        s.sorted(out, 5);
        TEST_ASSERT_EQUAL(int(AlertId::Stall), int(out[0].id));
        TEST_ASSERT_EQUAL(int(AlertLevel::Advisory), int(out[4].level));
    }
}

static void test_raise_results()
{
    AlertSet s;
    TEST_ASSERT_EQUAL(int(RaiseResult::Added), int(s.raise(AlertId::Test1, AlertLevel::Caution, "A")));
    TEST_ASSERT_EQUAL(int(RaiseResult::Same), int(s.raise(AlertId::Test1, AlertLevel::Caution, "A")));
    s.ackAll();
    TEST_ASSERT_EQUAL(int(RaiseResult::Updated), int(s.raise(AlertId::Test1, AlertLevel::Caution, "B")));
    TEST_ASSERT_FALSE(s.anyUnacked());   // text-only change: acked kept, no re-arm
    TEST_ASSERT_EQUAL(int(RaiseResult::Updated), int(s.raise(AlertId::Test1, AlertLevel::Advisory, "B")));
    TEST_ASSERT_FALSE(s.anyUnacked());   // lower level: no re-arm
    TEST_ASSERT_EQUAL(int(RaiseResult::Rearmed), int(s.raise(AlertId::Test1, AlertLevel::Warning, "B")));
    TEST_ASSERT_TRUE(s.anyUnacked());
    TEST_ASSERT_TRUE(s.anyUnackedWarning());
    Alert out[1];
    s.sorted(out, 1);
    TEST_ASSERT_EQUAL_STRING("B", out[0].text);
    TEST_ASSERT_EQUAL(int(AlertLevel::Warning), int(out[0].level));
}

static void test_clear_and_reoccurrence()
{
    AlertSet s;
    s.raise(AlertId::Stall, AlertLevel::Warning, "STALL");
    s.raise(AlertId::ClockNotSet, AlertLevel::Caution, "CLOCK NOT SET");
    TEST_ASSERT_TRUE(s.clear(AlertId::Stall));
    TEST_ASSERT_FALSE(s.clear(AlertId::Stall));
    Alert out[2];
    TEST_ASSERT_EQUAL(1, int(s.sorted(out, 2)));
    TEST_ASSERT_EQUAL(int(AlertId::ClockNotSet), int(out[0].id));
    TEST_ASSERT_EQUAL(1, int(s.ackAll()));
    TEST_ASSERT_EQUAL(0, int(s.ackAll()));   // nothing newly acknowledged
    // cleared then raised again: a new occurrence (unacked, later sequence)
    TEST_ASSERT_TRUE(s.clear(AlertId::ClockNotSet));
    s.raise(AlertId::SafeMode, AlertLevel::Caution, "SAFE MODE");
    TEST_ASSERT_EQUAL(int(RaiseResult::Added), int(s.raise(AlertId::ClockNotSet, AlertLevel::Caution, "CLOCK NOT SET")));
    TEST_ASSERT_TRUE(s.anyUnacked());
    s.sorted(out, 2);
    TEST_ASSERT_EQUAL(int(AlertId::SafeMode), int(out[0].id));
    TEST_ASSERT_FALSE(s.anyUnackedWarning());
}

static void test_ack_one()
{
    AlertSet s;
    s.raise(AlertId::Stall, AlertLevel::Warning, "STALL");
    s.raise(AlertId::Test1, AlertLevel::Caution, "T");
    TEST_ASSERT_TRUE(s.ack(AlertId::Stall));
    TEST_ASSERT_FALSE(s.anyUnackedWarning());
    TEST_ASSERT_TRUE(s.anyUnacked());   // Test1 still unacked
    TEST_ASSERT_FALSE(s.ack(AlertId::PullUp));
    TEST_ASSERT_TRUE(s.contains(AlertId::Stall));
    TEST_ASSERT_FALSE(s.contains(AlertId::PullUp));
}

static void test_capacity()
{
    AlertSet s;
    for (int i = 0; i < int(AlertSet::kCap); i++) {
        TEST_ASSERT_EQUAL(int(RaiseResult::Added), int(s.raise(AlertId(100 + i), AlertLevel::Advisory, "x")));
    }
    TEST_ASSERT_EQUAL(int(RaiseResult::Full), int(s.raise(AlertId(200), AlertLevel::Warning, "y")));
    TEST_ASSERT_EQUAL(int(AlertSet::kCap), int(s.count()));
    Alert out[16];
    TEST_ASSERT_EQUAL(16, int(s.sorted(out, 16)));
    TEST_ASSERT_EQUAL(24, int(s.count()));
}

static void test_text_truncated()
{
    AlertSet s;
    s.raise(AlertId::Test1, AlertLevel::Caution, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");
    Alert out[1];
    s.sorted(out, 1);
    TEST_ASSERT_EQUAL(24, int(strlen(out[0].text)));
    s.raise(AlertId::Test2, AlertLevel::Caution, nullptr);
}

static void test_table()
{
    const AlertDef* d = alertDef(AlertId::Stall);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(int(AlertLevel::Warning), int(d->level));
    TEST_ASSERT_EQUAL(int(Pattern::StickShaker), int(d->pattern));
    TEST_ASSERT_EQUAL(2000, d->repeatMs);
    TEST_ASSERT_EQUAL_STRING("STALL", d->text);
    d = alertDef(AlertId::PullUp);
    TEST_ASSERT_EQUAL(int(AlertLevel::Warning), int(d->level));
    TEST_ASSERT_EQUAL_STRING("PULL UP", d->text);
    TEST_ASSERT_EQUAL(int(AlertLevel::Advisory), int(alertDef(AlertId::TimeUnsync)->level));
    TEST_ASSERT_EQUAL(int(Pattern::Advisory), int(alertDef(AlertId::TimeUnsync)->pattern));
    TEST_ASSERT_EQUAL(int(AlertLevel::Caution), int(alertDef(AlertId::ClockNotSet)->level));
    TEST_ASSERT_EQUAL(0, alertDef(AlertId::ClockNotSet)->repeatMs);
    // peripheral range
    d = alertDef(AlertId(uint8_t(AlertId::PeriphFirst) + uint8_t(Periph::Imu)));
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(int(AlertLevel::Caution), int(d->level));
    char t[25];
    alertText(AlertId(uint8_t(Periph::Imu)), t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("IMU FAIL", t);
    alertText(AlertId::Stall, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("STALL", t);
    TEST_ASSERT_NULL(alertDef(AlertId(36)));    // LOW FUEL retired
    TEST_ASSERT_NULL(alertDef(AlertId(250)));
    TEST_ASSERT_EQUAL(int(Pattern::StickShaker), int(patternForLevel(AlertLevel::Warning)));
    TEST_ASSERT_EQUAL(int(Pattern::Caution), int(patternForLevel(AlertLevel::Caution)));
    TEST_ASSERT_EQUAL_STRING("WARNING", levelName(AlertLevel::Warning));
}

static void test_names()
{
    AlertId id;
    TEST_ASSERT_TRUE(alertIdFromName("stall", id));
    TEST_ASSERT_EQUAL(int(AlertId::Stall), int(id));
    TEST_ASSERT_TRUE(alertIdFromName("PULLUP", id));
    TEST_ASSERT_EQUAL(int(AlertId::PullUp), int(id));
    TEST_ASSERT_TRUE(alertIdFromName("test2", id));
    TEST_ASSERT_EQUAL(int(AlertId::Test2), int(id));
    TEST_ASSERT_TRUE(alertIdFromName("clocknotset", id));
    TEST_ASSERT_TRUE(alertIdFromName("imu", id));   // peripheral by name
    TEST_ASSERT_EQUAL(int(Periph::Imu), int(id));
    TEST_ASSERT_TRUE(alertIdFromName("40", id));   // numeric
    TEST_ASSERT_EQUAL(int(AlertId::Stall), int(id));
    TEST_ASSERT_FALSE(alertIdFromName("nope", id));
    TEST_ASSERT_FALSE(alertIdFromName("36", id));   // not in the table
    TEST_ASSERT_FALSE(alertIdFromName(nullptr, id));
    AlertLevel l;
    TEST_ASSERT_TRUE(levelFromLetter("W", l));
    TEST_ASSERT_EQUAL(int(AlertLevel::Warning), int(l));
    TEST_ASSERT_TRUE(levelFromLetter("a", l));
    TEST_ASSERT_EQUAL(int(AlertLevel::Advisory), int(l));
    TEST_ASSERT_FALSE(levelFromLetter("X", l));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_order_level_then_time);
    RUN_TEST(test_random_orders_warning_first);
    RUN_TEST(test_raise_results);
    RUN_TEST(test_clear_and_reoccurrence);
    RUN_TEST(test_ack_one);
    RUN_TEST(test_capacity);
    RUN_TEST(test_text_truncated);
    RUN_TEST(test_table);
    RUN_TEST(test_names);
    return UNITY_END();
}
