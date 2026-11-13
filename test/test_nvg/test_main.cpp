// core::NvgColor (black → #00FF66 ramp) and core::NvgStyle (alert looks in NVG). S7 data-model §5–6, FR-013/015.
#include <unity.h>
#include <initializer_list>
#include "core/NvgColor.h"
#include "core/NvgStyle.h"

using namespace core;

void setUp() {}
void tearDown() {}

static uint16_t rgb(uint8_t r5, uint8_t g6, uint8_t b5) { return uint16_t((r5 << 11) | (g6 << 5) | b5); }
static uint8_t G(uint16_t p) { return uint8_t((p >> 5) & 63); }
static uint8_t B(uint16_t p) { return uint8_t(p & 31); }
static uint8_t R(uint16_t p) { return uint8_t(p >> 11); }

static void test_endpoints()
{
    TEST_ASSERT_EQUAL_HEX16(0x0000, NvgColor::toGreen565(0x0000));
    TEST_ASSERT_EQUAL_HEX16(0x07EC, NvgColor::toGreen565(0xFFFF));   // #00FF66
}

static void test_mid_grey()
{
    uint8_t g = G(NvgColor::toGreen565(rgb(16, 32, 16)));
    TEST_ASSERT_INT_WITHIN(2, 32, g);
}

static void test_primaries_follow_luminance()
{
    TEST_ASSERT_INT_WITHIN(1, 19, G(NvgColor::toGreen565(rgb(31, 0, 0))));   // 0.299 × 63
    TEST_ASSERT_INT_WITHIN(1, 37, G(NvgColor::toGreen565(rgb(0, 63, 0))));   // 0.587 × 63
    TEST_ASSERT_INT_WITHIN(1, 7, G(NvgColor::toGreen565(rgb(0, 0, 31))));    // 0.114 × 63
}

static void test_all_inputs_on_ramp()
{
    for (uint32_t p = 0; p < 65536; p++) {
        uint16_t o = NvgColor::toGreen565(uint16_t(p));
        if (R(o) != 0) {
            TEST_FAIL_MESSAGE("red bits set");
        }
        uint8_t g = G(o);
        uint8_t expB = uint8_t((g * 12 + 31) / 63);
        if (B(o) != expB) {
            TEST_FAIL_MESSAGE("blue off the #00FF66 ramp");
        }
    }
}

static void test_monotonic_grey_ramp()
{
    uint8_t last = 0;
    for (uint8_t i = 0; i < 32; i++) {
        uint8_t g = G(NvgColor::toGreen565(rgb(i, uint8_t(i * 2 + (i == 31 ? 1 : 0)), i)));
        TEST_ASSERT_TRUE(g >= last);
        last = g;
    }
}

static void test_apply_matches_per_pixel()
{
    uint16_t buf[64], ref[64];
    for (int i = 0; i < 64; i++) {
        buf[i] = uint16_t(i * 1021u + 7u);
        ref[i] = NvgColor::toGreen565(buf[i]);
    }
    NvgColor::apply(buf, 64);
    TEST_ASSERT_EQUAL_HEX16_ARRAY(ref, buf, 64);
}

static void test_apply_swapped_matches()
{
    alignas(4) uint16_t buf[67];   // odd length: exercises the 32-bit path and the tail
    uint16_t ref[67];
    for (int i = 0; i < 67; i++) {
        buf[i] = uint16_t(i * 977u + 13u);
        uint16_t g = NvgColor::toGreen565(buf[i]);
        ref[i] = uint16_t((g >> 8) | (g << 8));
    }
    NvgColor::applySwapped(buf, 67);
    TEST_ASSERT_EQUAL_HEX16_ARRAY(ref, buf, 67);
    uint16_t* odd = buf + 1;   // unaligned start: per-pixel path
    for (int i = 0; i < 10; i++) {
        odd[i] = 0xFFFF;
    }
    NvgColor::applySwapped(odd, 10);
    TEST_ASSERT_EQUAL_HEX16(0xEC07, odd[0]);
    TEST_ASSERT_EQUAL_HEX16(0xEC07, odd[9]);

    // Exhaustive: the unclamped fast path equals the reference for every RGB565 value.
    static uint16_t all[65536];
    for (uint32_t p = 0; p < 65536; p++) {
        all[p] = uint16_t(p);
    }
    NvgColor::applySwapped(all, 65536);
    for (uint32_t p = 0; p < 65536; p++) {
        uint16_t g = NvgColor::toGreen565(uint16_t(p));
        if (all[p] != uint16_t((g >> 8) | (g << 8))) {
            TEST_FAIL_MESSAGE("fast path differs from reference");
        }
    }
}

static void test_style_day()
{
    for (AlertLevel l : {AlertLevel::Advisory, AlertLevel::Caution, AlertLevel::Warning}) {
        NvgStyle::AlertLook k = NvgStyle::forLevel(l, false, 260);
        TEST_ASSERT_FALSE(k.inverse);
        TEST_ASSERT_FALSE(k.outline);
        TEST_ASSERT_TRUE(k.visible);
    }
}

static void test_style_nvg()
{
    NvgStyle::AlertLook w0 = NvgStyle::forLevel(AlertLevel::Warning, true, 0);
    NvgStyle::AlertLook w1 = NvgStyle::forLevel(AlertLevel::Warning, true, 250);
    NvgStyle::AlertLook w2 = NvgStyle::forLevel(AlertLevel::Warning, true, 500);
    TEST_ASSERT_TRUE(w0.inverse);
    TEST_ASSERT_TRUE(w0.visible);
    TEST_ASSERT_FALSE(w1.visible);
    TEST_ASSERT_TRUE(w2.visible);
    for (uint32_t t : {0u, 250u, 750u}) {
        NvgStyle::AlertLook c = NvgStyle::forLevel(AlertLevel::Caution, true, t);
        TEST_ASSERT_TRUE(c.inverse);
        TEST_ASSERT_FALSE(c.outline);
        TEST_ASSERT_TRUE(c.visible);
        NvgStyle::AlertLook a = NvgStyle::forLevel(AlertLevel::Advisory, true, t);
        TEST_ASSERT_FALSE(a.inverse);
        TEST_ASSERT_TRUE(a.outline);
        TEST_ASSERT_TRUE(a.visible);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_endpoints);
    RUN_TEST(test_mid_grey);
    RUN_TEST(test_primaries_follow_luminance);
    RUN_TEST(test_all_inputs_on_ramp);
    RUN_TEST(test_monotonic_grey_ramp);
    RUN_TEST(test_apply_matches_per_pixel);
    RUN_TEST(test_apply_swapped_matches);
    RUN_TEST(test_style_day);
    RUN_TEST(test_style_nvg);
    return UNITY_END();
}
