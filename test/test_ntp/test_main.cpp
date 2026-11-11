#include <unity.h>
#include <cstring>
#include "core/Ntp.h"

using namespace core;

void setUp() {}
void tearDown() {}

// 2026-11-11T09:24:35Z in Unix seconds, and in NTP seconds (+2208988800).
static const int64_t kUnix = 1794389075;
static const uint32_t kNtpSec = uint32_t(kUnix + 2208988800LL);

static void put32(uint8_t* p, uint32_t v)
{
    p[0] = uint8_t(v >> 24);
    p[1] = uint8_t(v >> 16);
    p[2] = uint8_t(v >> 8);
    p[3] = uint8_t(v);
}

static void makeResponse(uint8_t* r, const uint8_t* req, uint32_t t2Sec, uint32_t t2Frac, uint32_t t3Sec,
                         uint32_t t3Frac)
{
    memset(r, 0, Ntp::kPacketSize);
    r[0] = 0x24;   // LI 0, VN 4, Mode 4 (server)
    r[1] = 2;      // stratum
    memcpy(r + 24, req + 40, 8);   // originate = our transmit timestamp
    put32(r + 32, t2Sec);
    put32(r + 36, t2Frac);
    put32(r + 40, t3Sec);
    put32(r + 44, t3Frac);
}

static void test_build_request()
{
    uint8_t req[Ntp::kPacketSize];
    Ntp::buildRequest(req, kUnix, 500);
    TEST_ASSERT_EQUAL_HEX8(0x23, req[0]);   // LI 0, VN 4, Mode 3 (client)
    TEST_ASSERT_EQUAL_HEX8(uint8_t(kNtpSec >> 24), req[40]);
    TEST_ASSERT_EQUAL_HEX8(uint8_t(kNtpSec), req[43]);
    TEST_ASSERT_EQUAL_HEX8(0x80, req[44]);   // 0.5 s fraction = 0x80000000
}

static void test_ntp_unix_conversion()
{
    TEST_ASSERT_EQUAL_INT64(kUnix, Ntp::ntpToUnix(kNtpSec));
    TEST_ASSERT_EQUAL_INT64(int64_t(3000000000LL) - 2208988800LL, Ntp::ntpToUnix(3000000000u));
}

static void test_parse_valid_with_symmetric_delay()
{
    uint8_t req[Ntp::kPacketSize], rsp[Ntp::kPacketSize];
    // Client clock is 10 s behind; request sent at client T1 = kUnix-10, reply received at T4 = T1 + 100 ms.
    Ntp::buildRequest(req, kUnix - 10, 0);
    // Server receives at kUnix + 50 ms, sends at kUnix + 50 ms (zero processing).
    const uint32_t frac50ms = 0x0CCCCCCC;
    makeResponse(rsp, req, kNtpSec, frac50ms, kNtpSec, frac50ms);
    Ntp::Result r{};
    TEST_ASSERT_TRUE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix - 10, 100, r));
    // True time at T4 = server send + half the round trip = kUnix + 0.050 + 0.050 = kUnix + 0.100
    TEST_ASSERT_EQUAL_INT64(kUnix, r.unixSec);
    TEST_ASSERT_INT_WITHIN(5, 100, r.ms);
    TEST_ASSERT_INT_WITHIN(5, 10000, r.offsetMs);   // client was 10 s slow
}

static void test_parse_rejects()
{
    uint8_t req[Ntp::kPacketSize], rsp[Ntp::kPacketSize];
    Ntp::Result r{};
    Ntp::buildRequest(req, kUnix, 0);

    makeResponse(rsp, req, kNtpSec, 0, kNtpSec, 0);
    rsp[0] = 0x23;   // mode 3, not a server reply
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));

    makeResponse(rsp, req, kNtpSec, 0, kNtpSec, 0);
    rsp[1] = 0;   // stratum 0 (kiss-o'-death)
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));
    rsp[1] = 16;
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));

    makeResponse(rsp, req, kNtpSec, 0, kNtpSec, 0);
    rsp[0] = 0xE4;   // LI 3 (clock unsynchronised)
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));

    makeResponse(rsp, req, kNtpSec, 0, kNtpSec, 0);
    rsp[24] ^= 0xFF;   // originate mismatch
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));

    makeResponse(rsp, req, kNtpSec, 0, 0, 0);   // transmit timestamp 0
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, sizeof rsp, req, kUnix, 0, r));

    makeResponse(rsp, req, kNtpSec, 0, kNtpSec, 0);
    TEST_ASSERT_FALSE(Ntp::parseResponse(rsp, 47, req, kUnix, 0, r));   // short packet
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_build_request);
    RUN_TEST(test_ntp_unix_conversion);
    RUN_TEST(test_parse_valid_with_symmetric_delay);
    RUN_TEST(test_parse_rejects);
    return UNITY_END();
}
