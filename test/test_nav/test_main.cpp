#include <unity.h>
#include "core/NavModel.h"

using core::NavModel;

void setUp() {}
void tearDown() {}

// Ring: 0 Home, 1 A, 2 B, 3 Settings
static void test_next_prev_wrap()
{
    NavModel m(4);
    TEST_ASSERT_EQUAL(0, m.current());
    TEST_ASSERT_EQUAL(1, m.next());
    m.transitionStarted(1);
    m.transitionDone();
    TEST_ASSERT_EQUAL(1, m.current());

    NavModel w(4);
    TEST_ASSERT_EQUAL(3, w.prev());   // wrap backwards from Home to Settings
}

static void test_full_ring_forward()
{
    NavModel m(4);
    int seq[] = {1, 2, 3, 0};
    for (int expected : seq) {
        int t = m.next();
        TEST_ASSERT_EQUAL(expected, t);
        m.transitionStarted(t);
        m.transitionDone();
    }
    TEST_ASSERT_EQUAL(0, m.current());
}

static void go(NavModel& m, int target)
{
    m.transitionStarted(target);
    TEST_ASSERT_EQUAL(-1, m.transitionDone());
}

static void test_button_from_non_home_goes_home_and_remembers()
{
    NavModel m(4);
    go(m, 2);   // at B
    TEST_ASSERT_EQUAL(0, m.button());
    go(m, 0);
    TEST_ASSERT_EQUAL(2, m.lastNonHome());
    TEST_ASSERT_EQUAL(2, m.button());   // from Home → last visited (B)
}

static void test_button_on_home_initially_goes_to_first_page()
{
    NavModel m(4);
    TEST_ASSERT_EQUAL(1, m.button());
}

static void test_button_on_home_tracks_latest_non_home()
{
    NavModel m(4);
    go(m, 3);   // Settings
    go(m, 0);   // swipe home
    TEST_ASSERT_EQUAL(3, m.button());
}

static void test_busy_ignores_swipes()
{
    NavModel m(4);
    int t = m.next();
    m.transitionStarted(t);
    TEST_ASSERT_TRUE(m.busy());
    TEST_ASSERT_EQUAL(-1, m.next());
    TEST_ASSERT_EQUAL(-1, m.prev());
    TEST_ASSERT_EQUAL(-1, m.transitionDone());
    TEST_ASSERT_FALSE(m.busy());
    TEST_ASSERT_EQUAL(1, m.current());
}

static void test_button_during_transition_ends_home()
{
    NavModel m(4);
    go(m, 1);
    int t = m.next();   // → B
    m.transitionStarted(t);
    TEST_ASSERT_EQUAL(-1, m.button());       // queued, not applied while busy
    TEST_ASSERT_EQUAL(0, m.transitionDone()); // pending home is returned as next target
    m.transitionStarted(0);
    TEST_ASSERT_EQUAL(-1, m.transitionDone());
    TEST_ASSERT_EQUAL(0, m.current());
    TEST_ASSERT_EQUAL(2, m.lastNonHome());
}

static void test_button_during_transition_to_home_needs_no_extra_hop()
{
    NavModel m(4);
    go(m, 3);
    int t = m.next();   // Settings → Home (wrap)
    TEST_ASSERT_EQUAL(0, t);
    m.transitionStarted(t);
    m.button();
    TEST_ASSERT_EQUAL(-1, m.transitionDone());
    TEST_ASSERT_EQUAL(0, m.current());
}

static void test_single_page_ring_is_inert()
{
    NavModel m(1);
    TEST_ASSERT_EQUAL(-1, m.next());
    TEST_ASSERT_EQUAL(-1, m.prev());
    TEST_ASSERT_EQUAL(-1, m.button());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_next_prev_wrap);
    RUN_TEST(test_full_ring_forward);
    RUN_TEST(test_button_from_non_home_goes_home_and_remembers);
    RUN_TEST(test_button_on_home_initially_goes_to_first_page);
    RUN_TEST(test_button_on_home_tracks_latest_non_home);
    RUN_TEST(test_busy_ignores_swipes);
    RUN_TEST(test_button_during_transition_ends_home);
    RUN_TEST(test_button_during_transition_to_home_needs_no_extra_hop);
    RUN_TEST(test_single_page_ring_is_inert);
    return UNITY_END();
}
