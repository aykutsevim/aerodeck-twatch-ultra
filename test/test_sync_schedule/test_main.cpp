#include <unity.h>
#include "core/SyncSchedule.h"

using core::SyncSchedule;

void setUp() {}
void tearDown() {}

static const uint32_t MIN = 60000;

static void test_boot()
{
    SyncSchedule s;
    s.onBoot(1000, true, 60);
    TEST_ASSERT_TRUE(s.due(1000));
    SyncSchedule n;
    n.onBoot(1000, false, 60);
    TEST_ASSERT_FALSE(n.due(1000));
    TEST_ASSERT_FALSE(n.due(1000 + 1000 * MIN));
}

static void test_success_waits_interval()
{
    SyncSchedule s;
    s.onBoot(0, true, 60);
    s.onSuccess(5000);
    TEST_ASSERT_FALSE(s.due(5000 + 59 * MIN));
    TEST_ASSERT_TRUE(s.due(5000 + 60 * MIN));
}

static void test_failure_backoff_capped()
{
    SyncSchedule s;
    s.onBoot(0, true, 60);
    s.onFailure(0);
    TEST_ASSERT_FALSE(s.due(2 * MIN - 1));
    TEST_ASSERT_TRUE(s.due(2 * MIN));
    s.onFailure(2 * MIN);
    TEST_ASSERT_TRUE(s.due(7 * MIN));    // +5
    s.onFailure(7 * MIN);
    TEST_ASSERT_TRUE(s.due(22 * MIN));   // +15
    s.onFailure(22 * MIN);
    TEST_ASSERT_FALSE(s.due(36 * MIN));  // stays at 15
    TEST_ASSERT_TRUE(s.due(37 * MIN));

    SyncSchedule c;   // backoff never longer than the interval
    c.onBoot(0, true, 15);
    c.onFailure(0);
    c.onFailure(0);
    c.onFailure(0);
    c.onFailure(0);
    TEST_ASSERT_TRUE(c.due(15 * MIN));
}

static void test_success_resets_backoff()
{
    SyncSchedule s;
    s.onBoot(0, true, 60);
    s.onFailure(0);
    s.onFailure(0);
    s.onSuccess(10 * MIN);
    s.onFailure(70 * MIN);
    TEST_ASSERT_TRUE(s.due(72 * MIN));   // back to the first backoff step (2 min)
}

static void test_configured_now_due()
{
    SyncSchedule s;
    s.onBoot(0, false, 60);
    s.onConfigured(100);
    TEST_ASSERT_TRUE(s.due(100));
}

static void test_interval_change()
{
    SyncSchedule s;
    s.onBoot(0, true, 60);
    s.onSuccess(0);
    s.setInterval(15);
    TEST_ASSERT_TRUE(s.due(15 * MIN));
}

static void test_is_unsynced()
{
    const int64_t now = 1794389075;
    TEST_ASSERT_TRUE(SyncSchedule::isUnsynced(now, 0, true));               // never synced
    TEST_ASSERT_FALSE(SyncSchedule::isUnsynced(now, now - 23 * 3600, true));
    TEST_ASSERT_TRUE(SyncSchedule::isUnsynced(now, now - 25 * 3600, true));
    TEST_ASSERT_FALSE(SyncSchedule::isUnsynced(now, 0, false));             // no network configured
}

static void test_millis_wraparound()
{
    SyncSchedule s;
    s.onBoot(0xFFFFFFF0u, true, 60);
    s.onSuccess(0xFFFFFFF0u);
    TEST_ASSERT_FALSE(s.due(0x00000100u));                 // just after wrap, not yet due
    TEST_ASSERT_TRUE(s.due(0xFFFFFFF0u + 60 * MIN));       // wraps correctly
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_boot);
    RUN_TEST(test_success_waits_interval);
    RUN_TEST(test_failure_backoff_capped);
    RUN_TEST(test_success_resets_backoff);
    RUN_TEST(test_configured_now_due);
    RUN_TEST(test_interval_change);
    RUN_TEST(test_is_unsynced);
    RUN_TEST(test_millis_wraparound);
    return UNITY_END();
}
