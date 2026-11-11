#include <unity.h>
#include "core/LockDetector.h"

using core::LockDetector;

void setUp() {}
void tearDown() {}

static void test_needs_500ms_in_band()
{
    LockDetector d;
    TEST_ASSERT_FALSE(d.update(0, 0, true, 1000));
    TEST_ASSERT_FALSE(d.update(1, 2, true, 1499));
    TEST_ASSERT_TRUE(d.update(1, 2, true, 1500));
}

static void test_small_tip_keeps_lock()
{
    LockDetector d;
    d.update(0, 0, true, 0);
    TEST_ASSERT_TRUE(d.update(0, 0, true, 600));
    TEST_ASSERT_TRUE(d.update(4, -4, true, 700));   // inside the ±5° hysteresis band
    TEST_ASSERT_TRUE(d.update(5, 0, true, 800));    // exactly 5 still locked
}

static void test_over_5_unlocks_immediately()
{
    LockDetector d;
    d.update(0, 0, true, 0);
    d.update(0, 0, true, 600);
    TEST_ASSERT_FALSE(d.update(5.1f, 0, true, 601));
    TEST_ASSERT_FALSE(d.update(0, -6, true, 602));
}

static void test_reentry_restarts_timer()
{
    LockDetector d;
    d.update(0, 0, true, 0);
    d.update(4, 0, true, 300);    // left the ±3° band (not yet locked)
    d.update(0, 0, true, 400);    // re-enter: timer restarts
    TEST_ASSERT_FALSE(d.update(0, 0, true, 800));
    TEST_ASSERT_TRUE(d.update(0, 0, true, 900));
}

static void test_status_not_ok_forces_unlock()
{
    LockDetector d;
    d.update(0, 0, true, 0);
    TEST_ASSERT_TRUE(d.update(0, 0, true, 600));
    TEST_ASSERT_FALSE(d.update(0, 0, false, 601));
    TEST_ASSERT_FALSE(d.update(0, 0, true, 700));   // timer restarted after status recovered
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_needs_500ms_in_band);
    RUN_TEST(test_small_tip_keeps_lock);
    RUN_TEST(test_over_5_unlocks_immediately);
    RUN_TEST(test_reentry_restarts_timer);
    RUN_TEST(test_status_not_ok_forces_unlock);
    return UNITY_END();
}
