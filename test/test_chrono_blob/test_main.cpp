// core::ChronoBlob: versioned, CRC-checked timer state (S8 data-model §3, FR-008).
#include <unity.h>
#include <cstring>
#include "core/ChronoBlob.h"

using namespace core;

void setUp() {}
void tearDown() {}

static ChronoModel::Raw sample()
{
    ChronoModel::Raw r{};
    r.mode = ChronoModel::Mode::Pomodoro;
    r.state = ChronoModel::State::Paused;
    r.phase = ChronoModel::Phase::Break;
    r.cycles = 7;
    r.late = true;
    r.intervalMs = 300000;
    r.endUtcMs = 1700000123456LL;
    r.remainingMs = 123456;
    r.countdownMs = 1500000;
    r.workMs = 1500000;
    r.breakMs = 300000;
    return r;
}

static void test_roundtrip()
{
    uint8_t buf[ChronoBlob::kSize];
    TEST_ASSERT_EQUAL(38, int(ChronoBlob::kSize));
    TEST_ASSERT_EQUAL(int(ChronoBlob::kSize), int(ChronoBlob::encode(sample(), buf)));
    ChronoModel::Raw o{};
    TEST_ASSERT_TRUE(ChronoBlob::decode(buf, sizeof buf, o));
    ChronoModel::Raw a = sample();
    TEST_ASSERT_EQUAL(int(a.mode), int(o.mode));
    TEST_ASSERT_EQUAL(int(a.state), int(o.state));
    TEST_ASSERT_EQUAL(int(a.phase), int(o.phase));
    TEST_ASSERT_EQUAL(a.cycles, o.cycles);
    TEST_ASSERT_EQUAL(a.late, o.late);
    TEST_ASSERT_EQUAL_INT64(a.intervalMs, o.intervalMs);
    TEST_ASSERT_EQUAL_INT64(a.endUtcMs, o.endUtcMs);
    TEST_ASSERT_EQUAL_INT64(a.remainingMs, o.remainingMs);
    TEST_ASSERT_EQUAL_INT64(a.countdownMs, o.countdownMs);
    TEST_ASSERT_EQUAL_INT64(a.workMs, o.workMs);
    TEST_ASSERT_EQUAL_INT64(a.breakMs, o.breakMs);
}

static void test_rejects()
{
    uint8_t buf[ChronoBlob::kSize];
    ChronoBlob::encode(sample(), buf);
    ChronoModel::Raw o{};
    o.cycles = 99;
    TEST_ASSERT_FALSE(ChronoBlob::decode(buf, sizeof buf - 1, o));
    TEST_ASSERT_FALSE(ChronoBlob::decode(nullptr, sizeof buf, o));
    uint8_t bad[ChronoBlob::kSize];
    memcpy(bad, buf, sizeof buf);
    bad[0] = 9;   // version
    TEST_ASSERT_FALSE(ChronoBlob::decode(bad, sizeof bad, o));
    memcpy(bad, buf, sizeof buf);
    bad[12] ^= 0x10;   // CRC
    TEST_ASSERT_FALSE(ChronoBlob::decode(bad, sizeof bad, o));
    memcpy(bad, buf, sizeof buf);
    bad[2] = 9;   // invalid state enum (CRC recomputed would still be invalid by range)
    TEST_ASSERT_FALSE(ChronoBlob::decode(bad, sizeof bad, o));
    TEST_ASSERT_EQUAL(99, o.cycles);   // untouched on failure
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_rejects);
    return UNITY_END();
}
