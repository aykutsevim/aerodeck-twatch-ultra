// core::DayLog: today + 7 previous days, local-midnight rollover rules (S6 data-model §2, FR-006..FR-008).
#include <unity.h>
#include "core/DayLog.h"

using namespace core;

void setUp() {}
void tearDown() {}

// 2026-11-12 as a day number (days since 1970-01-01)
static const int32_t D = 20769;

static void test_local_day()
{
    // 2026-11-12T20:59:59Z at +180 min = 23:59:59 local on the 12th; 21:00:00Z = 00:00 on the 13th
    const int64_t t = int64_t(D) * 86400 + 20 * 3600 + 59 * 60 + 59;
    TEST_ASSERT_EQUAL_INT32(D, DayLog::localDayOf(t, 180));
    TEST_ASSERT_EQUAL_INT32(D + 1, DayLog::localDayOf(t + 1, 180));
    TEST_ASSERT_EQUAL_INT32(D, DayLog::localDayOf(int64_t(D) * 86400 + 3600, -60));      // 00:00 local
    TEST_ASSERT_EQUAL_INT32(D - 1, DayLog::localDayOf(int64_t(D) * 86400 + 3599, -60));  // 23:59:59 the day before
    TEST_ASSERT_EQUAL_INT32(-1, DayLog::localDayOf(-1, 0));                               // floor before the epoch
    TEST_ASSERT_EQUAL(23 * 60 + 59, DayLog::minuteOfDay(t, 180));
    TEST_ASSERT_EQUAL(0, DayLog::minuteOfDay(t + 1, 180));
}

static void test_weekday()
{
    TEST_ASSERT_EQUAL(3, DayLog::weekday(0));     // 1970-01-01 Thursday (Monday = 0)
    TEST_ASSERT_EQUAL(3, DayLog::weekday(D));     // 2026-11-12 Thursday
    TEST_ASSERT_EQUAL(4, DayLog::weekday(D + 1));
    TEST_ASSERT_EQUAL(2, DayLog::weekday(-1));    // 1969-12-31 Wednesday
    int y, m, d;
    DayLog::civil(D, y, m, d);
    TEST_ASSERT_EQUAL(2026, y);
    TEST_ASSERT_EQUAL(11, m);
    TEST_ASSERT_EQUAL(12, d);
    DayLog::civil(0, y, m, d);
    TEST_ASSERT_EQUAL(1970, y);
    TEST_ASSERT_EQUAL(1, m);
    TEST_ASSERT_EQUAL(1, d);
    DayLog::civil(20513, y, m, d);   // 2026-02-28 + 1 = 2026-03-01
    TEST_ASSERT_EQUAL(3, m);
    TEST_ASSERT_EQUAL(1, d);
}

static void test_undated_then_dated()
{
    DayLog l;
    TEST_ASSERT_EQUAL_INT32(kUndated, l.today.day);
    l.addSteps(500);
    RollResult r = l.advance(false, D);
    TEST_ASSERT_FALSE(r.rolled);
    TEST_ASSERT_FALSE(r.dated);
    TEST_ASSERT_EQUAL_INT32(kUndated, l.today.day);
    r = l.advance(true, D);
    TEST_ASSERT_TRUE(r.dated);
    TEST_ASSERT_FALSE(r.rolled);
    TEST_ASSERT_EQUAL_INT32(D, l.today.day);
    TEST_ASSERT_EQUAL_UINT32(500, l.today.steps);   // counted values belong to the current date
}

static void test_next_day()
{
    DayLog l;
    l.advance(true, D);
    l.addSteps(1234);
    TEST_ASSERT_TRUE(l.notePeak(270, 845));
    RollResult r = l.advance(true, D);
    TEST_ASSERT_FALSE(r.rolled);
    r = l.advance(true, D + 1);
    TEST_ASSERT_TRUE(r.rolled);
    TEST_ASSERT_EQUAL(0, r.emptyDays);
    TEST_ASSERT_EQUAL_INT32(D + 1, l.today.day);
    TEST_ASSERT_EQUAL_UINT32(0, l.today.steps);
    TEST_ASSERT_EQUAL_UINT16(0, l.today.maxGc);
    TEST_ASSERT_EQUAL_INT16(-1, l.today.maxGMin);
    TEST_ASSERT_EQUAL_INT32(D, l.history[0].day);
    TEST_ASSERT_EQUAL_UINT32(1234, l.history[0].steps);
    TEST_ASSERT_EQUAL_UINT16(270, l.history[0].maxGc);
    TEST_ASSERT_EQUAL_INT16(845, l.history[0].maxGMin);
    TEST_ASSERT_EQUAL_INT32(kEmptyDay, l.history[1].day);
}

static void test_skipped_days()
{
    DayLog l;
    l.advance(true, D);
    l.addSteps(10);
    RollResult r = l.advance(true, D + 3);
    TEST_ASSERT_TRUE(r.rolled);
    TEST_ASSERT_EQUAL(2, r.emptyDays);
    TEST_ASSERT_EQUAL_INT32(D + 2, l.history[0].day);
    TEST_ASSERT_EQUAL_UINT32(0, l.history[0].steps);
    TEST_ASSERT_EQUAL_INT32(D + 1, l.history[1].day);
    TEST_ASSERT_EQUAL_INT32(D, l.history[2].day);
    TEST_ASSERT_EQUAL_UINT32(10, l.history[2].steps);

    DayLog m;
    m.advance(true, D);
    m.addSteps(10);
    r = m.advance(true, D + 20);
    TEST_ASSERT_EQUAL(7, r.emptyDays);
    for (int i = 0; i < DayLog::kHistory; i++) {
        TEST_ASSERT_EQUAL_INT32(D + 19 - i, m.history[i].day);   // all empty, the finished day is too old
        TEST_ASSERT_EQUAL_UINT32(0, m.history[i].steps);
    }
}

static void test_backward_and_same()
{
    DayLog l;
    l.advance(true, D);
    l.addSteps(42);
    RollResult r = l.advance(true, D - 1);   // clock corrected backwards
    TEST_ASSERT_FALSE(r.rolled);
    TEST_ASSERT_EQUAL_INT32(D, l.today.day);
    TEST_ASSERT_EQUAL_UINT32(42, l.today.steps);
    r = l.advance(true, D);
    TEST_ASSERT_FALSE(r.rolled);
}

static void test_cap_seven()
{
    DayLog l;
    l.advance(true, D);
    for (int i = 1; i <= 8; i++) {
        l.addSteps(uint32_t(i * 100));
        TEST_ASSERT_TRUE(l.advance(true, D + i).rolled);
    }
    // history[0] = day D+7 (800 steps) ... history[6] = day D+1 (200 steps); day D (100) dropped
    TEST_ASSERT_EQUAL_INT32(D + 7, l.history[0].day);
    TEST_ASSERT_EQUAL_UINT32(800, l.history[0].steps);
    TEST_ASSERT_EQUAL_INT32(D + 1, l.history[6].day);
    TEST_ASSERT_EQUAL_UINT32(200, l.history[6].steps);
}

static void test_peak()
{
    DayLog l;
    TEST_ASSERT_TRUE(l.notePeak(150, 600));
    TEST_ASSERT_FALSE(l.notePeak(120, 700));
    TEST_ASSERT_FALSE(l.notePeak(150, 701));
    TEST_ASSERT_EQUAL_UINT16(150, l.today.maxGc);
    TEST_ASSERT_EQUAL_INT16(600, l.today.maxGMin);
    TEST_ASSERT_TRUE(l.notePeak(310, 702));
    TEST_ASSERT_EQUAL_INT16(702, l.today.maxGMin);
}

static void test_day_at()
{
    DayLog l;
    l.advance(true, D);
    l.addSteps(5);
    l.advance(true, D + 2);   // one empty day (D+1) inserted
    l.addSteps(7);
    TEST_ASSERT_EQUAL_INT32(D + 2, l.dayAt(0).day);
    TEST_ASSERT_EQUAL_UINT32(7, l.dayAt(0).steps);
    TEST_ASSERT_EQUAL_INT32(D + 1, l.dayAt(1).day);
    TEST_ASSERT_EQUAL_UINT32(0, l.dayAt(1).steps);
    TEST_ASSERT_EQUAL_INT32(D, l.dayAt(2).day);
    TEST_ASSERT_EQUAL_UINT32(5, l.dayAt(2).steps);
    TEST_ASSERT_EQUAL_INT32(D - 4, l.dayAt(6).day);   // no data: an empty record carrying its date
    TEST_ASSERT_EQUAL_UINT32(0, l.dayAt(6).steps);
    l.clear();
    TEST_ASSERT_EQUAL_INT32(kUndated, l.today.day);
    TEST_ASSERT_EQUAL_INT32(kEmptyDay, l.history[0].day);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_local_day);
    RUN_TEST(test_weekday);
    RUN_TEST(test_undated_then_dated);
    RUN_TEST(test_next_day);
    RUN_TEST(test_skipped_days);
    RUN_TEST(test_backward_and_same);
    RUN_TEST(test_cap_seven);
    RUN_TEST(test_peak);
    RUN_TEST(test_day_at);
    return UNITY_END();
}
