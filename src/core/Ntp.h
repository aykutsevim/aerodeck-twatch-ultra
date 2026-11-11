// AeroDeck core: minimal SNTP (RFC 4330) packet codec (research R2, data-model §7). Pure, host-tested.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace Ntp {

constexpr size_t kPacketSize = 48;
constexpr int64_t kUnixToNtp = 2208988800LL;   // seconds from 1900-01-01 to 1970-01-01

struct Result {
    int64_t unixSec;    // corrected UTC at the moment the reply was received
    uint16_t ms;
    int64_t offsetMs;   // how far the local clock was behind (+) or ahead (−)
};

// Client request (LI 0, VN 4, Mode 3) with the transmit timestamp set to the local send time.
void buildRequest(uint8_t* buf, int64_t unixSec, uint16_t ms);
// NTP seconds (era 0 until 2036, era 1 after) → Unix seconds.
int64_t ntpToUnix(uint32_t ntpSec);
// Validates the reply against our request and computes the corrected time at receive time (nowUnixSec, nowMs).
bool parseResponse(const uint8_t* rsp, size_t len, const uint8_t* req, int64_t nowUnixSec, uint16_t nowMs, Result& out);

} // namespace Ntp
} // namespace core
