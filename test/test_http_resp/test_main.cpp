// core::HttpResp: bounded HTTP/1.1 response parser (S3 data-model §2, contracts/provider_http.md).
#include <unity.h>
#include <cstring>
#include <string>
#include "core/HttpResp.h"

using namespace core;
using HttpResp::State;

void setUp() {}
void tearDown() {}

static HttpResp::Result P(const std::string& s, bool closed = false)
{
    return HttpResp::parse(s.data(), s.size(), closed);
}

static const char* kOk =
    "HTTP/1.1 200 OK\r\n"
    "Date: Wed, 11 Nov 2026 12:58:58 GMT\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 63\r\n"
    "Connection: close\r\n"
    "Cache-control: max-age=60\r\n"
    "\r\n"
    "METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG";

static void test_observed_200()
{
    std::string s(kOk);
    HttpResp::Result r = P(s);
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_EQUAL(63, r.contentLength);
    TEST_ASSERT_FALSE(r.chunked);
    TEST_ASSERT_EQUAL(63, int(r.bodyLen));
    TEST_ASSERT_EQUAL(0, strncmp(s.data() + r.bodyOff, "METAR LTAC 111250Z", 18));
}

static void test_204_no_content()
{
    HttpResp::Result r = P("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(204, r.status);
    TEST_ASSERT_EQUAL(0, int(r.bodyLen));
}

static void test_chunked_flagged()
{
    HttpResp::Result r = P("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3f\r\nMETAR");
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_TRUE(r.chunked);
}

static void test_rejects_large_content_length()
{
    TEST_ASSERT_EQUAL(int(State::Error), int(P("HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n").state));
    TEST_ASSERT_EQUAL(int(State::Error), int(P("HTTP/1.1 200 OK\r\nContent-Length: 12a\r\n\r\nxx").state));
}

static void test_rejects_huge_headers()
{
    std::string s = "HTTP/1.1 200 OK\r\nX-Pad: " + std::string(1600, 'a') + "\r\n\r\nbody";
    TEST_ASSERT_EQUAL(int(State::Error), int(P(s).state));
    // Headers not terminated yet but already over the limit → error, not incomplete.
    std::string t = "HTTP/1.1 200 OK\r\nX-Pad: " + std::string(1600, 'a');
    TEST_ASSERT_EQUAL(int(State::Error), int(P(t).state));
}

static void test_partial_headers_incomplete()
{
    TEST_ASSERT_EQUAL(int(State::Incomplete), int(P("HTTP/1.1 200 OK\r\nContent-Le").state));
    TEST_ASSERT_EQUAL(int(State::Incomplete), int(P("").state));
    // Connection closed before the headers ended → error.
    TEST_ASSERT_EQUAL(int(State::Error), int(P("HTTP/1.1 200 OK\r\nContent-Le", true).state));
}

static void test_short_body()
{
    std::string s(kOk);
    s.resize(s.size() - 10);
    TEST_ASSERT_EQUAL(int(State::Incomplete), int(P(s).state));
    TEST_ASSERT_EQUAL(int(State::Error), int(P(s, true).state));   // closed early: truncated body is an error
}

static void test_no_content_length_reads_to_close()
{
    std::string s = "HTTP/1.1 200 OK\r\n\r\nMETAR LTAC 111250Z 27007KT";
    TEST_ASSERT_EQUAL(int(State::Incomplete), int(P(s).state));
    HttpResp::Result r = P(s, true);
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(-1, r.contentLength);
    TEST_ASSERT_EQUAL(26, int(r.bodyLen));   // "METAR LTAC 111250Z 27007KT"
    std::string big = "HTTP/1.1 200 OK\r\n\r\n" + std::string(2100, 'x');
    TEST_ASSERT_EQUAL(int(State::Error), int(P(big).state));
}

static void test_lf_only()
{
    std::string s = "HTTP/1.1 200 OK\nContent-Length: 4\n\nabcd";
    HttpResp::Result r = P(s);
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(4, int(r.bodyLen));
    TEST_ASSERT_EQUAL(0, strncmp(s.data() + r.bodyOff, "abcd", 4));
}

static void test_header_names_case_insensitive()
{
    HttpResp::Result r = P("HTTP/1.1 200 OK\r\ncontent-length: 2\r\n\r\nok");
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(2, r.contentLength);
}

static void test_garbage_and_errors()
{
    TEST_ASSERT_EQUAL(int(State::Error), int(P("HELLO\r\n\r\n").state));
    TEST_ASSERT_EQUAL(int(State::Error), int(P("HTTP/1.1 2x0 OK\r\n\r\n").state));
    HttpResp::Result r = P("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n");
    TEST_ASSERT_EQUAL(int(State::Complete), int(r.state));
    TEST_ASSERT_EQUAL(503, r.status);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_observed_200);
    RUN_TEST(test_204_no_content);
    RUN_TEST(test_chunked_flagged);
    RUN_TEST(test_rejects_large_content_length);
    RUN_TEST(test_rejects_huge_headers);
    RUN_TEST(test_partial_headers_incomplete);
    RUN_TEST(test_short_body);
    RUN_TEST(test_no_content_length_reads_to_close);
    RUN_TEST(test_lf_only);
    RUN_TEST(test_header_names_case_insensitive);
    RUN_TEST(test_garbage_and_errors);
    return UNITY_END();
}
