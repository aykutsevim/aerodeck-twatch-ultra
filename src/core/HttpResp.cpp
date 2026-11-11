#include "core/HttpResp.h"
#include <cstring>

namespace core {
namespace HttpResp {

namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; }

bool startsWithNoCase(const char* s, size_t n, const char* pre)
{
    size_t m = strlen(pre);
    if (n < m) {
        return false;
    }
    for (size_t i = 0; i < m; i++) {
        if (lower(s[i]) != lower(pre[i])) {
            return false;
        }
    }
    return true;
}

bool containsNoCase(const char* s, size_t n, const char* needle)
{
    size_t m = strlen(needle);
    for (size_t i = 0; i + m <= n; i++) {
        if (startsWithNoCase(s + i, n - i, needle)) {
            return true;
        }
    }
    return false;
}

Result err()
{
    Result r{};
    r.state = State::Error;
    r.contentLength = -1;
    return r;
}

} // namespace

Result parse(const char* buf, size_t len, bool closed)
{
    Result r{};
    r.state = State::Incomplete;
    r.contentLength = -1;
    if (!buf) {
        return closed ? err() : r;
    }

    // Find the end of the header block: "\r\n\r\n" or "\n\n".
    size_t hdrEnd = 0, bodyOff = 0;
    for (size_t i = 0; i + 1 < len && i < kMaxHeader + 4; i++) {
        if (buf[i] == '\n' && buf[i + 1] == '\n') {
            hdrEnd = i;
            bodyOff = i + 2;
            break;
        }
        if (i + 3 < len && buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            hdrEnd = i;
            bodyOff = i + 4;
            break;
        }
    }
    if (bodyOff == 0) {
        if (len > kMaxHeader || closed) {
            return err();
        }
        return r;   // headers not complete yet
    }
    if (hdrEnd > kMaxHeader) {
        return err();
    }

    // Status line: "HTTP/1.x NNN ..."
    if (hdrEnd < 12 || memcmp(buf, "HTTP/1.", 7) != 0 || buf[8] != ' ') {
        return err();
    }
    for (int k = 9; k < 12; k++) {
        if (buf[k] < '0' || buf[k] > '9') {
            return err();
        }
    }
    if (hdrEnd > 12 && buf[12] != ' ' && buf[12] != '\r' && buf[12] != '\n') {
        return err();
    }
    r.status = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + (buf[11] - '0');

    // Header lines.
    size_t p = 0;
    while (p < hdrEnd && buf[p] != '\n') {
        p++;
    }
    p++;
    while (p < hdrEnd) {
        size_t e = p;
        while (e < hdrEnd && buf[e] != '\n') {
            e++;
        }
        size_t le = e;
        if (le > p && buf[le - 1] == '\r') {
            le--;
        }
        const char* line = buf + p;
        const size_t n = le - p;
        if (startsWithNoCase(line, n, "content-length:")) {
            size_t v = 15;
            while (v < n && line[v] == ' ') {
                v++;
            }
            if (v >= n) {
                return err();
            }
            long cl = 0;
            for (; v < n; v++) {
                if (line[v] == ' ') {
                    break;
                }
                if (line[v] < '0' || line[v] > '9') {
                    return err();
                }
                cl = cl * 10 + (line[v] - '0');
                if (cl > kMaxBody) {
                    return err();
                }
            }
            r.contentLength = cl;
        } else if (startsWithNoCase(line, n, "transfer-encoding:")) {
            if (containsNoCase(line + 18, n - 18, "chunked")) {
                r.chunked = true;
            }
        }
        p = e + 1;
    }

    r.bodyOff = bodyOff;
    const size_t avail = len - bodyOff;
    if (r.chunked || r.status == 204 || r.status == 304 || (r.status >= 100 && r.status < 200)) {
        r.state = State::Complete;
        r.bodyLen = 0;
        return r;
    }
    if (r.contentLength >= 0) {
        if (avail >= size_t(r.contentLength)) {
            r.state = State::Complete;
            r.bodyLen = size_t(r.contentLength);
            return r;
        }
        return closed ? err() : r;
    }
    if (avail > size_t(kMaxBody)) {
        return err();
    }
    if (closed) {
        r.state = State::Complete;
        r.bodyLen = avail;
    }
    return r;
}

} // namespace HttpResp
} // namespace core
