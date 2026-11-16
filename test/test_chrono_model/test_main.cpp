// core::ChronoModel: countdown + Pomodoro on absolute UTC ms (S8 data-model §2, FR-001…011).
#include <unity.h>
#include "core/ChronoModel.h"

using namespace core;
using M = ChronoModel;

void setUp() {}
void tearDown() {}

static constexpr int64_t T0 = 1'700'000'000'000LL;   // some UTC ms
static constexpr int64_t S = 1000;

static ChronoModel countdown(int64_t ms)
{
    ChronoModel m;
    m.configure(M::Config{M::Mode::Countdown, ms, 25 * 60 * S, 5 * 60 * S});
    return m;
}

static ChronoModel pomo(int64_t work, int64_t brk)
{
    ChronoModel m;
    m.configure(M::Config{M::Mode::Pomodoro, 25 * 60 * S, work, brk});
    return m;
}

static void test_idle_shows_full()
{
    ChronoModel m = countdown(60 * S);
    TEST_ASSERT_EQUAL(int(M::State::Idle), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(60 * S, m.remainingMs(T0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, m.fraction(T0));
    TEST_ASSERT_EQUAL_INT64(-1, m.nextEventUtcMs());
}

static void test_countdown_expires_once()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    TEST_ASSERT_EQUAL(int(M::State::Running), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(T0 + 60 * S, m.nextEventUtcMs());
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.tick(T0 + 30 * S)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, m.fraction(T0 + 30 * S));
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.tick(T0 + 60 * S - 1)));
    TEST_ASSERT_EQUAL(int(M::Event::Expired), int(m.tick(T0 + 60 * S)));
    TEST_ASSERT_EQUAL(int(M::State::Expired), int(m.state()));
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.tick(T0 + 61 * S)));
    TEST_ASSERT_EQUAL_INT64(0, m.remainingMs(T0 + 61 * S));
    TEST_ASSERT_FALSE(m.late());
}

static void test_pause_resume_exact()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    m.pause(T0 + 10 * S + 250);
    TEST_ASSERT_EQUAL(int(M::State::Paused), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(49 * S + 750, m.remainingMs(T0 + 40 * S));   // frozen while paused
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.tick(T0 + 100 * S)));
    m.resume(T0 + 40 * S);
    TEST_ASSERT_EQUAL_INT64(49 * S + 750, m.remainingMs(T0 + 40 * S));
    TEST_ASSERT_EQUAL_INT64(T0 + 89 * S + 750, m.nextEventUtcMs());
}

static void test_start_while_paused_is_resume()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    m.pause(T0 + 20 * S);
    m.start(T0 + 50 * S);
    TEST_ASSERT_EQUAL(int(M::State::Running), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(40 * S, m.remainingMs(T0 + 50 * S));
}

static void test_reset()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    m.tick(T0 + 60 * S);
    m.reset();
    TEST_ASSERT_EQUAL(int(M::State::Idle), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(60 * S, m.remainingMs(T0 + 90 * S));
}

static void test_start_from_expired_clears_late()
{
    ChronoModel m = countdown(10 * S);
    m.start(T0);
    TEST_ASSERT_EQUAL(int(M::Event::Expired), int(m.restore(T0 + 100 * S)));
    TEST_ASSERT_TRUE(m.late());
    m.start(T0 + 200 * S);
    TEST_ASSERT_FALSE(m.late());
    TEST_ASSERT_EQUAL(int(M::State::Running), int(m.state()));
}

static void test_pomodoro_cycle()
{
    ChronoModel m = pomo(60 * S, 30 * S);
    m.start(T0);
    TEST_ASSERT_EQUAL(int(M::Phase::Work), int(m.phase()));
    // tick 400 ms late: the schedule must not drift
    TEST_ASSERT_EQUAL(int(M::Event::WorkDone), int(m.tick(T0 + 60 * S + 400)));
    TEST_ASSERT_EQUAL(int(M::Phase::Break), int(m.phase()));
    TEST_ASSERT_EQUAL(1, m.cycles());
    TEST_ASSERT_EQUAL_INT64(T0 + 90 * S, m.nextEventUtcMs());
    TEST_ASSERT_EQUAL_INT64(30 * S, m.intervalMs());
    TEST_ASSERT_EQUAL(int(M::Event::BreakDone), int(m.tick(T0 + 90 * S + 400)));
    TEST_ASSERT_EQUAL(int(M::Phase::Work), int(m.phase()));
    TEST_ASSERT_EQUAL_INT64(T0 + 150 * S, m.nextEventUtcMs());
    TEST_ASSERT_EQUAL(int(M::Event::WorkDone), int(m.tick(T0 + 150 * S)));
    TEST_ASSERT_EQUAL(2, m.cycles());
    TEST_ASSERT_EQUAL(int(M::State::Running), int(m.state()));
}

static void test_restore_running_future_unchanged()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.restore(T0 + 20 * S)));
    TEST_ASSERT_EQUAL_INT64(40 * S, m.remainingMs(T0 + 20 * S));
    TEST_ASSERT_FALSE(m.late());
}

static void test_restore_pomodoro_several_phases()
{
    ChronoModel m = pomo(60 * S, 30 * S);
    m.start(T0);
    // off for 200 s: W 0-60, B 60-90, W 90-150, B 150-180, W 180-240 → now in work, 2 cycles
    TEST_ASSERT_EQUAL(int(M::Event::BreakDone), int(m.restore(T0 + 200 * S)));
    TEST_ASSERT_EQUAL(int(M::Phase::Work), int(m.phase()));
    TEST_ASSERT_EQUAL(2, m.cycles());
    TEST_ASSERT_TRUE(m.late());
    TEST_ASSERT_EQUAL_INT64(T0 + 240 * S, m.nextEventUtcMs());
}

static void test_restore_paused_unchanged()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    m.pause(T0 + 15 * S);
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.restore(T0 + 3600 * S)));
    TEST_ASSERT_EQUAL(int(M::State::Paused), int(m.state()));
    TEST_ASSERT_EQUAL_INT64(45 * S, m.remainingMs(T0 + 3600 * S));
}

static void test_restore_clamps_after_clock_reset()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    // the clock came back far in the past (RTC lost): remaining must not exceed the interval
    const int64_t past = T0 - 1'000'000'000LL;
    TEST_ASSERT_EQUAL(int(M::Event::None), int(m.restore(past)));
    TEST_ASSERT_EQUAL_INT64(60 * S, m.remainingMs(past));
    TEST_ASSERT_EQUAL_INT64(past + 60 * S, m.nextEventUtcMs());
}

static void test_clock_correction_shifts_by_delta_only()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    // at true T0+20 s the clock is corrected 3 s forward: remaining drops by exactly 3 s
    TEST_ASSERT_EQUAL_INT64(37 * S, m.remainingMs(T0 + 23 * S));
}

static void test_configure_ignored_while_running()
{
    ChronoModel m = countdown(60 * S);
    m.start(T0);
    m.configure(M::Config{M::Mode::Pomodoro, 10 * S, 10 * S, 10 * S});
    TEST_ASSERT_EQUAL(int(M::Mode::Countdown), int(m.mode()));
    TEST_ASSERT_EQUAL_INT64(60 * S, m.intervalMs());
    m.reset();
    m.configure(M::Config{M::Mode::Pomodoro, 10 * S, 20 * S, 10 * S});
    TEST_ASSERT_EQUAL(int(M::Mode::Pomodoro), int(m.mode()));
    TEST_ASSERT_EQUAL_INT64(20 * S, m.remainingMs(T0));
}

static void test_raw_roundtrip()
{
    ChronoModel a = pomo(60 * S, 30 * S);
    a.start(T0);
    a.tick(T0 + 60 * S);
    ChronoModel b;
    b.load(a.raw());
    TEST_ASSERT_EQUAL(int(a.state()), int(b.state()));
    TEST_ASSERT_EQUAL(int(a.phase()), int(b.phase()));
    TEST_ASSERT_EQUAL(a.cycles(), b.cycles());
    TEST_ASSERT_EQUAL_INT64(a.nextEventUtcMs(), b.nextEventUtcMs());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_idle_shows_full);
    RUN_TEST(test_countdown_expires_once);
    RUN_TEST(test_pause_resume_exact);
    RUN_TEST(test_start_while_paused_is_resume);
    RUN_TEST(test_reset);
    RUN_TEST(test_start_from_expired_clears_late);
    RUN_TEST(test_pomodoro_cycle);
    RUN_TEST(test_restore_running_future_unchanged);
    RUN_TEST(test_restore_pomodoro_several_phases);
    RUN_TEST(test_restore_paused_unchanged);
    RUN_TEST(test_restore_clamps_after_clock_reset);
    RUN_TEST(test_clock_correction_shifts_by_delta_only);
    RUN_TEST(test_configure_ignored_while_running);
    RUN_TEST(test_raw_roundtrip);
    return UNITY_END();
}
