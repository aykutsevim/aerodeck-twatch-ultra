// core::SoundQueue (S5 data-model §4, FR-002, edge cases "many requests" and "same warning again").
#include <unity.h>
#include "core/SoundQueue.h"

using namespace core;

void setUp() {}
void tearDown() {}

static SoundReq R(Clip c, SoundPrio p, uint32_t t) { return SoundReq{c, p, t}; }

static void test_priority_then_fifo()
{
    SoundQueue q;
    TEST_ASSERT_EQUAL(int(PushResult::Queued), int(q.push(R(Clip::Chime, SoundPrio::Chime, 0))));
    q.push(R(Clip::MasterCaution, SoundPrio::Caution, 1));
    q.push(R(Clip::Minimums, SoundPrio::Caution, 2));
    q.push(R(Clip::Stall, SoundPrio::Warning, 3));
    q.push(R(Clip::Traffic, SoundPrio::Sample, 4));
    TEST_ASSERT_EQUAL(5, int(q.size()));
    SoundReq o;
    const Clip order[5] = {Clip::Stall, Clip::MasterCaution, Clip::Minimums, Clip::Chime, Clip::Traffic};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(q.pop(10, o));
        TEST_ASSERT_EQUAL(int(order[i]), int(o.clip));
    }
    TEST_ASSERT_FALSE(q.pop(10, o));
    TEST_ASSERT_EQUAL(0, int(q.size()));
}

static void test_duplicate()
{
    SoundQueue q;
    q.push(R(Clip::Stall, SoundPrio::Warning, 0));
    TEST_ASSERT_EQUAL(int(PushResult::Duplicate), int(q.push(R(Clip::Stall, SoundPrio::Warning, 100))));
    TEST_ASSERT_EQUAL(int(PushResult::Queued), int(q.push(R(Clip::Stall, SoundPrio::Sample, 100))));   // other prio
    TEST_ASSERT_EQUAL(2, int(q.size()));
}

static void test_full()
{
    SoundQueue q;
    const Clip c[6] = {Clip::MasterCaution, Clip::MasterWarning, Clip::Stall, Clip::PullUp, Clip::BingoFuel,
                       Clip::Minimums};
    for (int i = 0; i < 5; i++) q.push(R(c[i], SoundPrio::Caution, uint32_t(i)));
    q.push(R(c[5], SoundPrio::Sample, 5));
    TEST_ASSERT_EQUAL(int(SoundQueue::kCap), int(q.size()));
    // equal or lower than the lowest present → Full
    TEST_ASSERT_EQUAL(int(PushResult::Full), int(q.push(R(Clip::Traffic, SoundPrio::Sample, 6))));
    // higher → evicts the lowest (the Sample)
    TEST_ASSERT_EQUAL(int(PushResult::Queued), int(q.push(R(Clip::Traffic, SoundPrio::Warning, 7))));
    TEST_ASSERT_EQUAL(int(SoundQueue::kCap), int(q.size()));
    SoundReq o;
    TEST_ASSERT_TRUE(q.pop(8, o));
    TEST_ASSERT_EQUAL(int(Clip::Traffic), int(o.clip));
    int n = 0;
    while (q.pop(8, o)) {
        TEST_ASSERT_TRUE(o.clip != Clip::Minimums);
        n++;
    }
    TEST_ASSERT_EQUAL(5, n);
    // a full queue of Cautions refuses another Caution
    for (int i = 0; i < 6; i++) q.push(R(c[i], SoundPrio::Caution, 0));
    TEST_ASSERT_EQUAL(int(PushResult::Full), int(q.push(R(Clip::Traffic, SoundPrio::Caution, 0))));
}

static void test_stale()
{
    SoundQueue q;
    q.push(R(Clip::Stall, SoundPrio::Warning, 1000));
    q.push(R(Clip::Chime, SoundPrio::Chime, 1000));
    q.push(R(Clip::Minimums, SoundPrio::Sample, 1000));
    SoundReq o;
    // at +2500 ms: the chime (2000 ms max age) is gone; the others are still fresh
    TEST_ASSERT_TRUE(q.pop(3500, o));
    TEST_ASSERT_EQUAL(int(Clip::Stall), int(o.clip));
    TEST_ASSERT_TRUE(q.pop(3500, o));
    TEST_ASSERT_EQUAL(int(Clip::Minimums), int(o.clip));
    TEST_ASSERT_FALSE(q.pop(3500, o));

    q.push(R(Clip::Stall, SoundPrio::Warning, 1000));
    TEST_ASSERT_FALSE(q.pop(6001, o));   // older than 5000 ms
    TEST_ASSERT_EQUAL(0, int(q.size()));

    // millis wrap: requested just before the wrap, popped just after
    q.push(R(Clip::Stall, SoundPrio::Warning, 0xFFFFFF00u));
    TEST_ASSERT_TRUE(q.pop(0x00000100u, o));
    q.push(R(Clip::Stall, SoundPrio::Warning, 0xFFFFFF00u));
    TEST_ASSERT_FALSE(q.pop(0x00002000u, o));   // 8.4 s later
}

static void test_max_age()
{
    TEST_ASSERT_EQUAL_UINT32(2000, maxAgeMs(SoundPrio::Chime));
    TEST_ASSERT_EQUAL_UINT32(5000, maxAgeMs(SoundPrio::Warning));
    TEST_ASSERT_EQUAL_UINT32(5000, maxAgeMs(SoundPrio::Caution));
    TEST_ASSERT_EQUAL_UINT32(5000, maxAgeMs(SoundPrio::Sample));
}

static void test_purge()
{
    SoundQueue q;
    q.push(R(Clip::Stall, SoundPrio::Warning, 0));
    q.push(R(Clip::MasterCaution, SoundPrio::Caution, 0));
    q.push(R(Clip::Chime, SoundPrio::Chime, 0));
    q.push(R(Clip::Minimums, SoundPrio::Sample, 0));
    q.purge(SoundPrio::Caution);
    TEST_ASSERT_EQUAL(2, int(q.size()));
    SoundReq o;
    TEST_ASSERT_TRUE(q.pop(1, o));
    TEST_ASSERT_EQUAL(int(Clip::Chime), int(o.clip));
    TEST_ASSERT_TRUE(q.pop(1, o));
    TEST_ASSERT_EQUAL(int(Clip::Minimums), int(o.clip));
    q.push(R(Clip::Minimums, SoundPrio::Sample, 0));
    q.clear();
    TEST_ASSERT_EQUAL(0, int(q.size()));
}

static void test_preempt()
{
    TEST_ASSERT_TRUE(shouldPreempt(SoundPrio::Chime, SoundPrio::Warning));
    TEST_ASSERT_TRUE(shouldPreempt(SoundPrio::Caution, SoundPrio::Warning));
    TEST_ASSERT_TRUE(shouldPreempt(SoundPrio::Sample, SoundPrio::Chime));
    TEST_ASSERT_FALSE(shouldPreempt(SoundPrio::Warning, SoundPrio::Warning));
    TEST_ASSERT_FALSE(shouldPreempt(SoundPrio::Warning, SoundPrio::Caution));
    TEST_ASSERT_FALSE(shouldPreempt(SoundPrio::Caution, SoundPrio::Chime));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_priority_then_fifo);
    RUN_TEST(test_duplicate);
    RUN_TEST(test_full);
    RUN_TEST(test_stale);
    RUN_TEST(test_max_age);
    RUN_TEST(test_purge);
    RUN_TEST(test_preempt);
    return UNITY_END();
}
