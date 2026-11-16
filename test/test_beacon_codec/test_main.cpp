// core::BeaconCodec: SAS §7.2 beacon v1 (S9 data-model §3, FR-001, FR-006).
#include <unity.h>
#include <cstring>
#include "core/BeaconCodec.h"

using namespace core;

void setUp() {}
void tearDown() {}

static Beacon sample()
{
    Beacon b{};
    b.nodeId = 0x1A2B3C4Du;
    b.seq = 200;
    strcpy(b.callsign, "AD-9F00");
    b.battPct = 88;
    return b;
}

static void test_layout()
{
    uint8_t buf[BeaconCodec::kMaxSize];
    memset(buf, 0xEE, sizeof(buf));
    const Beacon b = sample();
    TEST_ASSERT_EQUAL(18, int(BeaconCodec::encode(b, buf)));
    const uint8_t want[18] = {0xAD, 0x01, 0x01, 0x4D, 0x3C, 0x2B, 0x1A, 200,
                              'A',  'D',  '-',  '9',  'F',  '0',  '0',  0x00, 88, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, buf, 18);
    TEST_ASSERT_EQUAL_HEX8(0xEE, buf[18]);   // nothing written past the end
}

static void test_roundtrip()
{
    uint8_t buf[BeaconCodec::kMaxSize];
    const Beacon b = sample();
    const size_t n = BeaconCodec::encode(b, buf);
    Beacon o{};
    TEST_ASSERT_TRUE(BeaconCodec::decode(buf, n, o));
    TEST_ASSERT_EQUAL_HEX32(b.nodeId, o.nodeId);
    TEST_ASSERT_EQUAL(b.seq, o.seq);
    TEST_ASSERT_EQUAL_STRING("AD-9F00", o.callsign);
    TEST_ASSERT_EQUAL(88, o.battPct);
    TEST_ASSERT_FALSE(o.hasPos);
}

static void test_position_variant()
{
    Beacon b = sample();
    b.hasPos = true;
    b.lat24 = -1234567;
    b.lon24 = 4000000;
    uint8_t buf[BeaconCodec::kMaxSize];
    TEST_ASSERT_EQUAL(24, int(BeaconCodec::encode(b, buf)));
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[17]);
    Beacon o{};
    TEST_ASSERT_TRUE(BeaconCodec::decode(buf, 24, o));
    TEST_ASSERT_TRUE(o.hasPos);
    TEST_ASSERT_EQUAL(-1234567, o.lat24);
    TEST_ASSERT_EQUAL(4000000, o.lon24);
}

static void test_lowercase_uppercased()
{
    Beacon b = sample();
    strcpy(b.callsign, "ab-1");
    uint8_t buf[BeaconCodec::kMaxSize];
    const size_t n = BeaconCodec::encode(b, buf);
    Beacon o{};
    TEST_ASSERT_TRUE(BeaconCodec::decode(buf, n, o));
    TEST_ASSERT_EQUAL_STRING("AB-1", o.callsign);
    buf[8] = 'z';   // a lowercase byte on the wire is uppercased too
    TEST_ASSERT_TRUE(BeaconCodec::decode(buf, n, o));
    TEST_ASSERT_EQUAL('Z', o.callsign[0]);
}

static void expectReject(uint8_t* buf, size_t n)
{
    Beacon o = sample();
    o.seq = 7;
    TEST_ASSERT_FALSE(BeaconCodec::decode(buf, n, o));
    TEST_ASSERT_EQUAL(7, o.seq);   // untouched on failure
    TEST_ASSERT_EQUAL_STRING("AD-9F00", o.callsign);
}

static void test_rejects()
{
    uint8_t good[BeaconCodec::kMaxSize];
    const size_t n = BeaconCodec::encode(sample(), good);
    uint8_t buf[BeaconCodec::kMaxSize];

    memcpy(buf, good, n);
    expectReject(buf, 17);                       // short
    expectReject(buf, 20);                       // neither 18 nor 24
    Beacon tmp{};
    TEST_ASSERT_FALSE(BeaconCodec::decode(nullptr, 18, tmp));
    memcpy(buf, good, n); buf[0] = 0xAC; expectReject(buf, n);   // magic
    memcpy(buf, good, n); buf[1] = 0x02; expectReject(buf, n);   // version
    memcpy(buf, good, n); buf[2] = 0x07; expectReject(buf, n);   // type
    memcpy(buf, good, n); buf[9] = 0x07; expectReject(buf, n);   // non-printable callsign
    memcpy(buf, good, n); buf[9] = '!'; expectReject(buf, n);    // not [A-Z0-9- ]
    memcpy(buf, good, n); memset(buf + 8, ' ', 8); expectReject(buf, n);   // empty callsign
    memcpy(buf, good, n); memset(buf + 8, 0, 8); expectReject(buf, n);      // empty (NUL) callsign
    memcpy(buf, good, n); buf[11] = 0; expectReject(buf, n);                 // padding inside the callsign
    {
        memcpy(buf, good, n);
        buf[15] = ' ';   // a space-padded sender is tolerated
        Beacon o{};
        TEST_ASSERT_TRUE(BeaconCodec::decode(buf, n, o));
        TEST_ASSERT_EQUAL_STRING("AD-9F00", o.callsign);
    }
    memcpy(buf, good, n); buf[16] = 101; expectReject(buf, n);   // battery > 100
    memcpy(buf, good, n); buf[17] = 0x01; expectReject(buf, n);  // has-position flag but 18 bytes
    uint8_t big[24] = {0};
    memcpy(big, good, n);
    expectReject(big, 24);                       // 24 bytes but no position flag
}

static void test_meshtastic_hook()
{
    uint8_t buf[BeaconCodec::kMaxSize];
    const size_t n = BeaconCodec::encode(sample(), buf);
    TEST_ASSERT_FALSE(BeaconCodec::isMeshtastic(buf, n));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_layout);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_position_variant);
    RUN_TEST(test_lowercase_uppercased);
    RUN_TEST(test_rejects);
    RUN_TEST(test_meshtastic_hook);
    return UNITY_END();
}
