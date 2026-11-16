// core::TcasTable: contacts, RSSI rings, bearing, ageing (S9 data-model §6, research R5, FR-006, FR-007, FR-010).
#include <unity.h>
#include <cstring>
#include "core/TcasTable.h"

using namespace core;

void setUp() {}
void tearDown() {}

static Beacon bc(uint32_t id, const char* call = "ALPHA", uint8_t batt = 50)
{
    Beacon b{};
    b.nodeId = id;
    strncpy(b.callsign, call, 8);
    b.battPct = batt;
    return b;
}

static void test_ring_for()
{
    TEST_ASSERT_EQUAL(0, TcasTable::ringFor(-30.0f));
    TEST_ASSERT_EQUAL(0, TcasTable::ringFor(-40.0f));
    TEST_ASSERT_EQUAL(0, TcasTable::ringFor(-54.0f));
    TEST_ASSERT_EQUAL(1, TcasTable::ringFor(-55.0f));
    TEST_ASSERT_EQUAL(2, TcasTable::ringFor(-70.0f));
    TEST_ASSERT_EQUAL(3, TcasTable::ringFor(-85.0f));
    TEST_ASSERT_EQUAL(3, TcasTable::ringFor(-120.0f));
}

static void test_bearing_stable_and_spread()
{
    TEST_ASSERT_EQUAL(TcasTable::bearingFor(0x11111111u), TcasTable::bearingFor(0x11111111u));
    bool seen[4] = {false, false, false, false};
    for (uint32_t id = 1; id <= 64; id++) {
        const uint16_t b = TcasTable::bearingFor(id);
        TEST_ASSERT_TRUE(b < 360);
        seen[b / 90] = true;
    }
    for (bool s : seen) TEST_ASSERT_TRUE(s);   // consecutive ids land in every quadrant
}

static void test_ema()
{
    TcasTable t;
    bool in = false;
    int i = t.update(bc(1), -100, 20, 1000, false, in);
    TEST_ASSERT_TRUE(i >= 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, t.at(i).rssiEma);   // new contact = first sample
    t.update(bc(1), -50, 20, 2000, false, in);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -85.0f, t.at(i).rssiEma);    // moves 30 %
    TEST_ASSERT_EQUAL(3, t.at(i).ring);
}

static void test_update_keeps_fields()
{
    TcasTable t;
    bool in = false;
    const int i = t.update(bc(0xABCD, "BRAVO", 77), -70, 30, 5000, true, in);
    const Contact& c = t.at(i);
    TEST_ASSERT_EQUAL_HEX32(0xABCD, c.nodeId);
    TEST_ASSERT_EQUAL_STRING("BRAVO", c.callsign);
    TEST_ASSERT_EQUAL(77, c.battPct);
    TEST_ASSERT_EQUAL(-70, c.lastRssi);
    TEST_ASSERT_EQUAL(30, c.lastSnrQ4);
    TEST_ASSERT_EQUAL_UINT32(5000, c.lastSeenMs);
    TEST_ASSERT_TRUE(c.simulated);
    TEST_ASSERT_EQUAL(TcasTable::bearingFor(0xABCD), c.bearingDeg);
    TEST_ASSERT_EQUAL(1, t.count());
    TEST_ASSERT_EQUAL(i, t.find(0xABCD));
    TEST_ASSERT_EQUAL(-1, t.find(0x9999));
}

static void test_entered_inner()
{
    TcasTable t;
    bool in = false;
    t.update(bc(1), -40, 0, 0, false, in);
    TEST_ASSERT_TRUE(in);                                  // new at -40
    t.update(bc(1), -40, 0, 1000, false, in);
    TEST_ASSERT_FALSE(in);                                 // staying
    for (int k = 0; k < 10; k++) t.update(bc(1), -90, 0, 2000 + k, false, in);
    TEST_ASSERT_FALSE(in);
    TEST_ASSERT_TRUE(t.at(t.find(1)).ring >= 1);           // left
    bool any = false;
    for (int k = 0; k < 10; k++) {
        t.update(bc(1), -40, 0, 3000 + k, false, in);
        any = any || in;
        if (in) break;
    }
    TEST_ASSERT_TRUE(any);                                 // back → true again
    TEST_ASSERT_TRUE(t.anyInner());
    // aged out then back → true
    TEST_ASSERT_EQUAL(1, t.age(400000, 300000, nullptr, 0));
    TEST_ASSERT_EQUAL(0, t.count());
    TEST_ASSERT_FALSE(t.anyInner());
    t.update(bc(1), -40, 0, 400001, false, in);
    TEST_ASSERT_TRUE(in);
}

static void test_age_removes_only_stale()
{
    TcasTable t;
    bool in;
    t.update(bc(1, "A"), -80, 0, 0, false, in);
    t.update(bc(2, "B"), -80, 0, 100000, false, in);
    uint32_t lost[4];
    TEST_ASSERT_EQUAL(1, t.age(300000, 300000, lost, 4));
    TEST_ASSERT_EQUAL_HEX32(1, lost[0]);
    TEST_ASSERT_EQUAL(1, t.count());
    TEST_ASSERT_EQUAL(-1, t.find(1));
    TEST_ASSERT_TRUE(t.find(2) >= 0);
}

static void test_full_replaces_oldest()
{
    TcasTable t;
    bool in;
    for (uint32_t id = 1; id <= TcasTable::kMax; id++) {
        t.update(bc(id), -80, 0, id * 10, false, in);
    }
    TEST_ASSERT_EQUAL(TcasTable::kMax, t.count());
    t.update(bc(1), -80, 0, 1000, false, in);   // refresh id 1 → id 2 is now the oldest
    t.update(bc(100), -80, 0, 2000, false, in);
    TEST_ASSERT_EQUAL(TcasTable::kMax, t.count());
    TEST_ASSERT_EQUAL(-1, t.find(2));
    TEST_ASSERT_TRUE(t.find(1) >= 0);
    TEST_ASSERT_TRUE(t.find(100) >= 0);
}

static void test_remove_simulated()
{
    TcasTable t;
    bool in;
    t.update(bc(1), -80, 0, 0, true, in);
    t.update(bc(2), -80, 0, 0, false, in);
    t.update(bc(3), -80, 0, 0, true, in);
    TEST_ASSERT_EQUAL(2, t.removeSimulated());
    TEST_ASSERT_EQUAL(1, t.count());
    TEST_ASSERT_TRUE(t.find(2) >= 0);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_ring_for);
    RUN_TEST(test_bearing_stable_and_spread);
    RUN_TEST(test_ema);
    RUN_TEST(test_update_keeps_fields);
    RUN_TEST(test_entered_inner);
    RUN_TEST(test_age_removes_only_stale);
    RUN_TEST(test_full_replaces_oldest);
    RUN_TEST(test_remove_simulated);
    return UNITY_END();
}
