// core::AudioPolicy: quiet hours, the allow matrix, the hourly chime rule (S5 data-model §5, FR-009, FR-010,
// FR-012, FR-016).
#include <unity.h>
#include <cstring>
#include "core/AudioPolicy.h"

using namespace core;

void setUp() {}
void tearDown() {}

// local seconds for an arbitrary day (day 20000 since the epoch) at h:m:s
static int64_t at(int h, int m, int s) { return int64_t(20000) * 86400 + h * 3600 + m * 60 + s; }

static ChimeDecision due(int64_t t, int64_t last = -1)
{
    return chimeDue(t, true, true, false, 2, 22, 7, last);
}

static void test_quiet_hours()
{
    TEST_ASSERT_FALSE(inQuietHours(21, 22, 7));
    TEST_ASSERT_TRUE(inQuietHours(22, 22, 7));
    TEST_ASSERT_TRUE(inQuietHours(23, 22, 7));
    TEST_ASSERT_TRUE(inQuietHours(0, 22, 7));
    TEST_ASSERT_TRUE(inQuietHours(6, 22, 7));
    TEST_ASSERT_FALSE(inQuietHours(7, 22, 7));
    TEST_ASSERT_FALSE(inQuietHours(22, 23, 6));
    TEST_ASSERT_TRUE(inQuietHours(23, 23, 6));
    TEST_ASSERT_FALSE(inQuietHours(6, 23, 6));
    for (int h = 0; h < 24; h++) TEST_ASSERT_FALSE(inQuietHours(h, 0, 0));   // start == end: none
    TEST_ASSERT_FALSE(inQuietHours(6, 7, 22));                                // daytime window
    TEST_ASSERT_TRUE(inQuietHours(7, 7, 22));
    TEST_ASSERT_TRUE(inQuietHours(21, 7, 22));
    TEST_ASSERT_FALSE(inQuietHours(22, 7, 22));
}

static void test_allow_matrix()
{
    const SoundPrio all[4] = {SoundPrio::Sample, SoundPrio::Chime, SoundPrio::Caution, SoundPrio::Warning};
    for (SoundPrio p : all) {
        TEST_ASSERT_TRUE(allow(p, false, 2, false));
        TEST_ASSERT_FALSE(allow(p, true, 2, false));    // quiet mode: nothing
        TEST_ASSERT_FALSE(allow(p, false, 0, false));   // volume Off: nothing
        TEST_ASSERT_EQUAL(p == SoundPrio::Warning, allow(p, false, 2, true));   // quiet hours: warnings only
    }
    TEST_ASSERT_EQUAL_STRING("quiet", denyReason(SoundPrio::Warning, true, 0, true));
    TEST_ASSERT_EQUAL_STRING("volume off", denyReason(SoundPrio::Warning, false, 0, true));
    TEST_ASSERT_EQUAL_STRING("quiet hours", denyReason(SoundPrio::Chime, false, 3, true));
    TEST_ASSERT_NULL(denyReason(SoundPrio::Warning, false, 3, true));
}

static void test_chime_window()
{
    ChimeDecision d = due(at(14, 0, 0));
    TEST_ASSERT_TRUE(d.due);
    TEST_ASSERT_EQUAL_INT64(20000 * 24 + 14, d.hourKey);
    TEST_ASSERT_TRUE(due(at(14, 0, 4)).due);
    d = due(at(14, 0, 5));
    TEST_ASSERT_FALSE(d.due);
    TEST_ASSERT_EQUAL_STRING("not-hh00", chimeReasonName(d.reason));
    TEST_ASSERT_FALSE(due(at(14, 30, 0)).due);
    TEST_ASSERT_FALSE(due(at(6, 59, 59)).due);
    TEST_ASSERT_TRUE(due(at(7, 0, 0)).due);
    TEST_ASSERT_EQUAL_STRING("-", chimeReasonName(due(at(7, 0, 0)).reason));
}

static void test_chime_once_per_hour()
{
    ChimeDecision d = due(at(14, 0, 0));
    ChimeDecision e = due(at(14, 0, 1), d.hourKey);
    TEST_ASSERT_FALSE(e.due);
    TEST_ASSERT_EQUAL_STRING("already", chimeReasonName(e.reason));
    // a clock jump back into 15:00:02 after chiming at 15:00:00 → no second chime
    ChimeDecision f = due(at(15, 0, 0), d.hourKey);
    TEST_ASSERT_TRUE(f.due);
    TEST_ASSERT_FALSE(due(at(15, 0, 2), f.hourKey).due);
    // a jump from 13:59 to 16:20 → no catch-up chime
    TEST_ASSERT_FALSE(due(at(16, 20, 0), due(at(13, 0, 0)).hourKey).due);
    // the next day, same hour → due again
    TEST_ASSERT_TRUE(due(at(14, 0, 0) + 86400, d.hourKey).due);
}

static void test_chime_gates()
{
    ChimeDecision d = due(at(23, 0, 0));
    TEST_ASSERT_FALSE(d.due);
    TEST_ASSERT_EQUAL_STRING("quiet-hours", chimeReasonName(d.reason));
    TEST_ASSERT_EQUAL_STRING("quiet-hours", chimeReasonName(due(at(23, 0, 1)).reason));
    TEST_ASSERT_TRUE(due(at(22, 0, 0) - 3600).due);   // 21:00

    d = chimeDue(at(14, 0, 0), false, true, false, 2, 22, 7, -1);
    TEST_ASSERT_FALSE(d.due);
    TEST_ASSERT_EQUAL_STRING("clock", chimeReasonName(d.reason));
    d = chimeDue(at(14, 0, 0), true, false, false, 2, 22, 7, -1);
    TEST_ASSERT_EQUAL_STRING("disabled", chimeReasonName(d.reason));
    TEST_ASSERT_FALSE(d.due);
    d = chimeDue(at(14, 0, 0), true, true, true, 2, 22, 7, -1);
    TEST_ASSERT_EQUAL_STRING("quiet", chimeReasonName(d.reason));
    TEST_ASSERT_FALSE(d.due);
    d = chimeDue(at(14, 0, 0), true, true, false, 0, 22, 7, -1);
    TEST_ASSERT_EQUAL_STRING("volume", chimeReasonName(d.reason));
    TEST_ASSERT_FALSE(d.due);
    // quiet hours 23–6: 22:00 chimes, 23:00 does not
    TEST_ASSERT_TRUE(chimeDue(at(22, 0, 0), true, true, false, 2, 23, 6, -1).due);
    TEST_ASSERT_FALSE(chimeDue(at(23, 0, 0), true, true, false, 2, 23, 6, -1).due);
}

static void test_hour_of_day()
{
    TEST_ASSERT_EQUAL(14, localHourOf(at(14, 0, 0)));
    TEST_ASSERT_EQUAL(0, localHourOf(at(0, 0, 0)));
    TEST_ASSERT_EQUAL(23, localHourOf(-1));   // before the epoch still yields 0..23
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_quiet_hours);
    RUN_TEST(test_allow_matrix);
    RUN_TEST(test_chime_window);
    RUN_TEST(test_chime_once_per_hour);
    RUN_TEST(test_chime_gates);
    RUN_TEST(test_hour_of_day);
    return UNITY_END();
}
