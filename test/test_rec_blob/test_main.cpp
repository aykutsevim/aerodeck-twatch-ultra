// core::RecBlob: versioned, CRC-checked persistence record (S6 data-model §5, FR-009).
#include <unity.h>
#include <cstring>
#include "core/RecBlob.h"

using namespace core;

void setUp() {}
void tearDown() {}

static DayLog sample()
{
    DayLog l;
    l.advance(true, 20404);
    l.addSteps(1234);
    l.notePeak(271, 845);
    l.advance(true, 20406);   // one empty day
    l.addSteps(99);
    l.notePeak(1600, 1);
    return l;
}

static void test_roundtrip()
{
    DayLog a = sample();
    uint8_t buf[RecBlob::kSize];
    TEST_ASSERT_EQUAL(100, int(RecBlob::kSize));
    TEST_ASSERT_EQUAL(int(RecBlob::kSize), int(RecBlob::encode(a, buf)));
    DayLog b;
    TEST_ASSERT_TRUE(RecBlob::decode(buf, sizeof buf, b));
    TEST_ASSERT_EQUAL_INT32(a.today.day, b.today.day);
    TEST_ASSERT_EQUAL_UINT32(99, b.today.steps);
    TEST_ASSERT_EQUAL_UINT16(1600, b.today.maxGc);
    TEST_ASSERT_EQUAL_INT16(1, b.today.maxGMin);
    for (int i = 0; i < DayLog::kHistory; i++) {
        TEST_ASSERT_EQUAL_INT32(a.history[i].day, b.history[i].day);
        TEST_ASSERT_EQUAL_UINT32(a.history[i].steps, b.history[i].steps);
        TEST_ASSERT_EQUAL_UINT16(a.history[i].maxGc, b.history[i].maxGc);
        TEST_ASSERT_EQUAL_INT16(a.history[i].maxGMin, b.history[i].maxGMin);
    }
    TEST_ASSERT_EQUAL_INT32(kEmptyDay, b.history[3].day);

    DayLog u;   // undated today round-trips too
    u.addSteps(7);
    RecBlob::encode(u, buf);
    DayLog v;
    TEST_ASSERT_TRUE(RecBlob::decode(buf, sizeof buf, v));
    TEST_ASSERT_EQUAL_INT32(kUndated, v.today.day);
    TEST_ASSERT_EQUAL_UINT32(7, v.today.steps);
}

static void test_rejects()
{
    DayLog a = sample();
    uint8_t buf[RecBlob::kSize];
    RecBlob::encode(a, buf);
    DayLog out;
    out.addSteps(5);
    TEST_ASSERT_FALSE(RecBlob::decode(buf, sizeof buf - 1, out));   // short
    TEST_ASSERT_FALSE(RecBlob::decode(nullptr, sizeof buf, out));
    uint8_t bad[RecBlob::kSize];
    memcpy(bad, buf, sizeof buf);
    bad[0] = 2;   // unknown version
    TEST_ASSERT_FALSE(RecBlob::decode(bad, sizeof bad, out));
    memcpy(bad, buf, sizeof buf);
    bad[20] ^= 0x01;   // flipped bit → CRC mismatch
    TEST_ASSERT_FALSE(RecBlob::decode(bad, sizeof bad, out));
    TEST_ASSERT_EQUAL_UINT32(5, out.today.steps);   // untouched on failure
}

static void test_crc()
{
    const uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, RecBlob::crc16(s, sizeof s));   // CRC-16/CCITT-FALSE check value
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_rejects);
    RUN_TEST(test_crc);
    return UNITY_END();
}
