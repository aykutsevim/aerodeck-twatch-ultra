// core::ScreenPolicy: screen timeout, wake causes, touch suppression (S7 data-model §2, FR-001…005).
#include <unity.h>
#include "core/ScreenPolicy.h"

using namespace core;
using A = ScreenPolicy::Action;
using C = ScreenPolicy::Cause;

void setUp() {}
void tearDown() {}

static ScreenPolicy make(uint8_t idx = 1, bool raise = true, bool avail = true, uint32_t now = 0)
{
    ScreenPolicy p;
    p.configure(idx, raise, avail);
    p.begin(now);
    return p;
}

static void test_timeout_table()
{
    const uint32_t exp[] = {10000, 15000, 30000, 60000, 300000, 0};
    for (uint8_t i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_UINT32(exp[i], ScreenPolicy::timeoutMs(i));
    }
    TEST_ASSERT_EQUAL_UINT32(15000, ScreenPolicy::timeoutMs(99));   // out of range → default
}

static void test_turns_off_at_timeout()
{
    ScreenPolicy p = make();
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(14999, 14999, false, false)));
    TEST_ASSERT_TRUE(p.on());
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(15000, 15000, false, false)));
    TEST_ASSERT_FALSE(p.on());
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(16000, 16000, false, false)));
}

static void test_touch_keeps_on()
{
    ScreenPolicy p = make();
    // the last touch was 2 s ago at t = 20 s: idle = min(inactive, since wake) = 2 s
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(20000, 2000, false, false)));
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(33000, 15000, false, false)));
}

static void test_wake_gives_full_timeout()
{
    ScreenPolicy p = make();
    p.update(15000, 15000, false, false);   // off
    TEST_ASSERT_TRUE(p.onRaise(20000));
    TEST_ASSERT_EQUAL(int(A::TurnOn), int(p.update(20000, 20000, false, false)));
    TEST_ASSERT_EQUAL(int(C::Raise), int(p.cause()));
    // no touch at all since: inactive keeps growing, but the wake restarts the idle time
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(34999, 34999, false, false)));
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(35000, 35000, false, false)));
}

static void test_never()
{
    ScreenPolicy p = make(5);
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(3600000, 3600000, false, false)));
    TEST_ASSERT_TRUE(p.on());
}

static void test_setup_screen_blocks_off()
{
    ScreenPolicy p = make();
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(60000, 60000, true, false)));
    TEST_ASSERT_TRUE(p.on());
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(60050, 60050, false, false)));
}

static void test_button()
{
    ScreenPolicy p = make();
    TEST_ASSERT_FALSE(p.onButton(5000));                       // on: not consumed …
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(19999, 19999, false, false)));   // … and restarts idle
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(20000, 20000, false, false)));
    TEST_ASSERT_TRUE(p.onButton(25000));                       // off: consumed
    TEST_ASSERT_EQUAL(int(A::TurnOn), int(p.update(25000, 25000, false, false)));
    TEST_ASSERT_EQUAL(int(C::Button), int(p.cause()));
}

static void test_raise_rules()
{
    ScreenPolicy p = make(1, false, true);
    p.update(15000, 15000, false, false);
    TEST_ASSERT_FALSE(p.onRaise(16000));   // disabled
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(16000, 16000, false, false)));
    p.configure(1, true, false);
    TEST_ASSERT_FALSE(p.onRaise(17000));   // unavailable
    p.configure(1, true, true);
    TEST_ASSERT_TRUE(p.onRaise(18000));
    TEST_ASSERT_EQUAL(int(A::TurnOn), int(p.update(18000, 18000, false, false)));
    TEST_ASSERT_FALSE(p.onRaise(18500));   // already on: ignored
}

static void test_alerts()
{
    ScreenPolicy p = make();
    p.update(15000, 15000, false, false);
    TEST_ASSERT_FALSE(p.onAlert(AlertLevel::Advisory, 16000));
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(16000, 16000, false, false)));
    TEST_ASSERT_TRUE(p.onAlert(AlertLevel::Caution, 17000));
    TEST_ASSERT_EQUAL(int(A::TurnOn), int(p.update(17000, 17000, false, false)));
    TEST_ASSERT_EQUAL(int(C::Alert), int(p.cause()));
    // on: a warning extends the timeout
    p.onAlert(AlertLevel::Warning, 30000);
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(44999, 44999, false, false)));
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(45000, 45000, false, false)));
}

static void test_timeout_change_applies_next_update()
{
    ScreenPolicy p = make(5);
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(20000, 20000, false, false)));
    p.configure(0, true, true);   // 10 s
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(20001, 20001, false, false)));
}

static void test_touch_suppression_after_wake()
{
    ScreenPolicy p = make();
    p.update(15000, 15000, false, false);
    p.onButton(20000);
    p.update(20000, 20000, false, true);   // finger already on the glass
    TEST_ASSERT_TRUE(p.suppressTouch());
    p.update(20500, 20500, false, true);   // still held after 300 ms: still suppressed
    TEST_ASSERT_TRUE(p.suppressTouch());
    p.update(20600, 0, false, false);      // released
    TEST_ASSERT_FALSE(p.suppressTouch());

    ScreenPolicy q = make();
    q.update(15000, 15000, false, false);
    q.onButton(20000);
    q.update(20000, 20000, false, false);
    TEST_ASSERT_TRUE(q.suppressTouch());
    q.update(20299, 20299, false, false);
    TEST_ASSERT_TRUE(q.suppressTouch());
    q.update(20300, 20300, false, false);   // 300 ms with no touch down
    TEST_ASSERT_FALSE(q.suppressTouch());
}

static void test_command()
{
    ScreenPolicy p = make();
    p.command(false, 1000);
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(1000, 1000, false, false)));
    p.command(true, 2000);
    TEST_ASSERT_EQUAL(int(A::TurnOn), int(p.update(2000, 2000, false, false)));
    TEST_ASSERT_EQUAL(int(C::Cmd), int(p.cause()));
}

static void test_millis_wrap()
{
    ScreenPolicy p = make(0, true, true, 0xFFFFF000u);   // 4096 ms before the wrap
    TEST_ASSERT_EQUAL(int(A::None), int(p.update(0x00001000u, 8192, false, false)));   // 8.2 s
    TEST_ASSERT_EQUAL(int(A::TurnOff), int(p.update(0x00001800u, 10240, false, false)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_timeout_table);
    RUN_TEST(test_turns_off_at_timeout);
    RUN_TEST(test_touch_keeps_on);
    RUN_TEST(test_wake_gives_full_timeout);
    RUN_TEST(test_never);
    RUN_TEST(test_setup_screen_blocks_off);
    RUN_TEST(test_button);
    RUN_TEST(test_raise_rules);
    RUN_TEST(test_alerts);
    RUN_TEST(test_timeout_change_applies_next_update);
    RUN_TEST(test_touch_suppression_after_wake);
    RUN_TEST(test_command);
    RUN_TEST(test_millis_wrap);
    return UNITY_END();
}
