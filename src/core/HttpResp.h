// AeroDeck core: bounded HTTP/1.1 response parser for the weather fetch (S3 data-model §2,
// contracts/provider_http.md). Pure, host-tested (test/test_http_resp). Input is untrusted (FR-020).
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace HttpResp {

constexpr size_t kMaxHeader = 1536;
constexpr long kMaxBody = 2048;

enum class State : uint8_t { Incomplete, Complete, Error };

struct Result {
    State state;
    int status;           // 0 until the status line is parsed
    long contentLength;   // -1 when absent
    bool chunked;         // Transfer-Encoding: chunked (not supported: caller treats as an error)
    size_t bodyOff;
    size_t bodyLen;
};

// Parses what has been received so far. `closed` = the server closed the connection (no more bytes).
// Complete: headers ended and the body is all there (or the status has no body).
// Incomplete: read more. Error: malformed, over the limits, or closed too early.
Result parse(const char* buf, size_t len, bool closed);

} // namespace HttpResp
} // namespace core
