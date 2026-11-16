// core::DutyCycle: rolling-hour transmit budget (S9 data-model §5, research R4, FR-003, SC-003).
#include <unity.h>
#include <cstring>
#include "core/DutyCycle.h"

using namespace core;

void setUp() {}
void tearDown() {}

static constexpr int64_t T0 = 1763300000000LL;   // some UTC ms
static constexpr int64_t H = 3600000;

static void test_empty_can_send()
{
    DutyCycle d;
    TEST_ASSERT_TRUE(d.canSend(T0, 185));
    TEST_ASSERT_EQUAL_UINT32(0, d.usedMs(T0));
    TEST_ASSERT_TRUE(d.nextAllowedMs(T0, 185) == T0);
}

static void test_sum_and_expiry()
{
    DutyCycle d;
    d.record(T0, 1000);
    d.record(T0 + 1000, 2000);
    TEST_ASSERT_EQUAL_UINT32(3000, d.usedMs(T0 + 2000));
    TEST_ASSERT_EQUAL_UINT32(2000, d.usedMs(T0 + H));        // the first record has left the window
    TEST_ASSERT_EQUAL_UINT32(0, d.usedMs(T0 + 1000 + H));
}

static void test_boundary()
{
    DutyCycle d;
    d.record(T0, 35000);
    TEST_ASSERT_TRUE(d.canSend(T0 + 1, 1000));    // 36 000 exactly = allowed
    TEST_ASSERT_FALSE(d.canSend(T0 + 1, 1001));   // +1 ms refused
}

static void test_next_allowed()
{
    DutyCycle d;
    d.record(T0, 20000);
    d.record(T0 + 60000, 16000);
    TEST_ASSERT_FALSE(d.canSend(T0 + 120000, 185));
    const int64_t n = d.nextAllowedMs(T0 + 120000, 185);
    TEST_ASSERT_TRUE(n == T0 + H);                // the first record drops out
    TEST_ASSERT_TRUE(d.canSend(n, 185));
    TEST_ASSERT_FALSE(d.canSend(n - 1, 185));
}

static void test_soak_5s_never_over_budget()
{
    DutyCycle d;
    int sent = 0;
    uint32_t maxUsed = 0;
    for (int64_t t = T0; t < T0 + 2 * H; t += 5000) {
        if (d.canSend(t, 185)) {
            d.record(t, 185);
            if (t < T0 + H) sent++;
        }
        const uint32_t u = d.usedMs(t);
        if (u > maxUsed) maxUsed = u;
    }
    TEST_ASSERT_TRUE(maxUsed <= 36000);
    TEST_ASSERT_INT_WITHIN(1, 194, sent);         // 36 000 / 185
}

static void test_full_ring_is_conservative()
{
    DutyCycle d(H, 100000);                        // a big budget so only the ring limits
    for (int i = 0; i < DutyCycle::kMaxRecords; i++) {
        TEST_ASSERT_TRUE(d.canSend(T0 + i, 10));
        d.record(T0 + i, 10);
    }
    TEST_ASSERT_FALSE(d.canSend(T0 + 1000, 10));  // full and the oldest still counts → refuse
    TEST_ASSERT_TRUE(d.canSend(T0 + H, 10));      // the oldest left the window
}

static void test_save_restore()
{
    DutyCycle d;
    d.record(T0, 185);
    d.record(T0 + 5000, 185);
    d.record(T0 + 10000, 300);
    uint8_t buf[DutyCycle::kBlobMax];
    const size_t n = d.save(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    DutyCycle r;
    TEST_ASSERT_TRUE(r.restore(buf, n, T0 + 20000));
    TEST_ASSERT_EQUAL_UINT32(670, r.usedMs(T0 + 20000));
    // a corrupt blob → false and empty
    buf[5] ^= 0x40;
    DutyCycle bad;
    bad.record(T0, 100);
    TEST_ASSERT_FALSE(bad.restore(buf, n, T0 + 20000));
    TEST_ASSERT_EQUAL_UINT32(0, bad.usedMs(T0 + 20000));
    // bad magic
    buf[5] ^= 0x40;
    buf[0] ^= 0xFF;
    TEST_ASSERT_FALSE(r.restore(buf, n, T0));
    TEST_ASSERT_FALSE(r.restore(nullptr, n, T0));
    TEST_ASSERT_FALSE(r.restore(buf, 3, T0));
}

static void test_future_records_clamped()
{
    DutyCycle d;
    d.record(T0 + H, 1000);                        // dated an hour ahead (the clock was set back)
    uint8_t buf[DutyCycle::kBlobMax];
    const size_t n = d.save(buf, sizeof(buf));
    DutyCycle r;
    TEST_ASSERT_TRUE(r.restore(buf, n, T0));
    TEST_ASSERT_EQUAL_UINT32(1000, r.usedMs(T0));        // counts now
    TEST_ASSERT_EQUAL_UINT32(1000, r.usedMs(T0 + H - 1)); // and for a full hour from now
    TEST_ASSERT_EQUAL_UINT32(0, r.usedMs(T0 + H));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_can_send);
    RUN_TEST(test_sum_and_expiry);
    RUN_TEST(test_boundary);
    RUN_TEST(test_next_allowed);
    RUN_TEST(test_soak_5s_never_over_budget);
    RUN_TEST(test_full_ring_is_conservative);
    RUN_TEST(test_save_restore);
    RUN_TEST(test_future_records_clamped);
    return UNITY_END();
}
