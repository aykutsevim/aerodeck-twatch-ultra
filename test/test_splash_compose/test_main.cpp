// Feature 011: boot splash composition (core::SplashCompose) and the generated KAAN assets.
#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "assets/KaanArt.h"
#include "core/SplashCompose.h"

using namespace core;

void setUp() {}
void tearDown() {}

static uint16_t sw(uint16_t v) { return uint16_t((v >> 8) | (v << 8)); }

// 4x2 mask: alpha ramp 0, 85, 170, 255 in both rows.
static const uint8_t kRamp[8] = {0, 85, 170, 255, 0, 85, 170, 255};

static void test_alpha_ends_and_monotonic()
{
    uint16_t out[4];
    SplashCompose::row(kRamp, 4, 2, 4, 2, 0, 0xFFFF, false, out);
    TEST_ASSERT_EQUAL_HEX16(0x0000, out[0]);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, out[3]);
    // each channel grows with alpha
    for (int i = 1; i < 4; i++) {
        TEST_ASSERT_TRUE((out[i] >> 11) >= (out[i - 1] >> 11));
        TEST_ASSERT_TRUE(((out[i] >> 5) & 0x3F) >= ((out[i - 1] >> 5) & 0x3F));
        TEST_ASSERT_TRUE((out[i] & 0x1F) >= (out[i - 1] & 0x1F));
    }
    // a coloured fg: alpha 255 gives exactly fg; alpha 0 gives black
    SplashCompose::row(kRamp, 4, 2, 4, 2, 1, 0x07E0, false, out);
    TEST_ASSERT_EQUAL_HEX16(0x07E0, out[3]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, out[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, out[1] & 0xF81F);   // no red/blue leaks into a pure green
}

static void test_rows_outside_art_are_black()
{
    uint16_t out[10];
    // 4x2 art centred in 10x6: rows 0,1 and 4,5 are outside
    for (uint16_t y : {0, 1, 4, 5}) {
        memset(out, 0xAB, sizeof out);
        SplashCompose::row(kRamp, 4, 2, 10, 6, y, 0xFFFF, false, out);
        for (int x = 0; x < 10; x++) {
            TEST_ASSERT_EQUAL_HEX16(0, out[x]);
        }
    }
}

static void test_centring_even_and_odd()
{
    uint16_t out[11];
    // even difference: W=10, aw=4 -> x0 = 3; H=6, ah=2 -> y0 = 2
    SplashCompose::row(kRamp, 4, 2, 10, 6, 2, 0xFFFF, false, out);
    TEST_ASSERT_EQUAL_HEX16(0, out[2]);
    TEST_ASSERT_EQUAL_HEX16(0, out[3]);        // alpha 0 at art x=0
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, out[6]);   // alpha 255 at art x=3
    TEST_ASSERT_EQUAL_HEX16(0, out[7]);
    // odd difference: W=11 -> x0 = 3 (floor)
    SplashCompose::row(kRamp, 4, 2, 11, 6, 3, 0xFFFF, false, out);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, out[6]);
    TEST_ASSERT_EQUAL_HEX16(0, out[7]);
}

static void test_byte_swap()
{
    uint16_t a[4], b[4];
    SplashCompose::row(kRamp, 4, 2, 4, 2, 0, 0xF800, false, a);
    SplashCompose::row(kRamp, 4, 2, 4, 2, 0, 0xF800, true, b);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX16(sw(a[i]), b[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(0x00F8, b[3]);
}

static void test_fits_rounded()
{
    static uint8_t full[20 * 20];
    memset(full, 255, sizeof full);
    // a 20x20 solid block centred on a 100x100 screen with r=10 fits
    TEST_ASSERT_TRUE(SplashCompose::fitsRounded(full, 20, 20, 100, 100, 10, 0));
    // a full-screen solid block does not: its corners are outside the arcs
    static uint8_t big[100 * 100];
    memset(big, 255, sizeof big);
    TEST_ASSERT_FALSE(SplashCompose::fitsRounded(big, 100, 100, 100, 100, 10, 0));
    // ...but it fits once its corner pixels are transparent enough (a rounded mask)
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            const int cx = x < 10 ? 10 : (x > 89 ? 89 : x), cy = y < 10 ? 10 : (y > 89 ? 89 : y);
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > 64) {
                big[y * 100 + x] = 0;
            }
        }
    }
    TEST_ASSERT_TRUE(SplashCompose::fitsRounded(big, 100, 100, 100, 100, 10, 0));
    // the margin shrinks the allowed area
    TEST_ASSERT_TRUE(SplashCompose::fitsRounded(full, 20, 20, 30, 30, 0, 5));
    TEST_ASSERT_FALSE(SplashCompose::fitsRounded(full, 20, 20, 30, 30, 0, 6));
}

static void test_should_show_rule()
{
    SplashSkip why = SplashSkip::None;
    TEST_ASSERT_TRUE(SplashCompose::shouldShow(BootMode::Normal, WakeCause::PowerOn, true, why));
    TEST_ASSERT_EQUAL(int(SplashSkip::None), int(why));
    for (WakeCause w : {WakeCause::Button, WakeCause::Imu, WakeCause::Timer, WakeCause::Other}) {
        TEST_ASSERT_FALSE(SplashCompose::shouldShow(BootMode::Normal, w, true, why));
        TEST_ASSERT_EQUAL(int(SplashSkip::Wake), int(why));
    }
    for (BootMode m : {BootMode::Safe, BootMode::Headless}) {
        TEST_ASSERT_FALSE(SplashCompose::shouldShow(m, WakeCause::PowerOn, true, why));
        TEST_ASSERT_EQUAL(int(SplashSkip::Safe), int(why));
    }
    TEST_ASSERT_FALSE(SplashCompose::shouldShow(BootMode::Normal, WakeCause::PowerOn, false, why));
    TEST_ASSERT_EQUAL(int(SplashSkip::NoArt), int(why));
    TEST_ASSERT_EQUAL_STRING("wake", splashSkipName(SplashSkip::Wake));
    TEST_ASSERT_EQUAL_STRING("safe", splashSkipName(SplashSkip::Safe));
    TEST_ASSERT_EQUAL_STRING("none", splashSkipName(SplashSkip::NoArt));
    TEST_ASSERT_EQUAL_STRING("nomem", splashSkipName(SplashSkip::NoMem));
}

static void test_kaan_assets_sane()
{
    TEST_ASSERT_TRUE(kKaanSplash.w > 0 && kKaanSplash.h > 0 && kKaanSplash.a8);
    TEST_ASSERT_TRUE(kKaanEmblem.w > 0 && kKaanEmblem.h > 0 && kKaanEmblem.a8);
    TEST_ASSERT_TRUE(kKaanSplash.w <= 410 && kKaanSplash.h <= 502);
    const float as = float(kKaanSplash.w) / kKaanSplash.h, ae = float(kKaanEmblem.w) / kKaanEmblem.h;
    TEST_ASSERT_FLOAT_WITHIN(0.02f * as, as, ae);   // the emblem is a downscale of the same crop
    TEST_ASSERT_TRUE(SplashCompose::fitsRounded(kKaanSplash.a8, kKaanSplash.w, kKaanSplash.h, 410, 502, 64, 0));
    // the art has real content: some full-white and some black pixels
    int white = 0, black = 0;
    for (int i = 0; i < kKaanSplash.w * kKaanSplash.h; i++) {
        white += kKaanSplash.a8[i] == 255;
        black += kKaanSplash.a8[i] == 0;
    }
    TEST_ASSERT_TRUE(white > 1000 && black > 1000);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_alpha_ends_and_monotonic);
    RUN_TEST(test_rows_outside_art_are_black);
    RUN_TEST(test_centring_even_and_odd);
    RUN_TEST(test_byte_swap);
    RUN_TEST(test_fits_rounded);
    RUN_TEST(test_should_show_rule);
    RUN_TEST(test_kaan_assets_sane);
    return UNITY_END();
}
