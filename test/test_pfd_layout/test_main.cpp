#include <unity.h>
#include "assets/KaanArt.h"
#include "core/PfdLayout.h"

using core::Box;
using core::PfdLayout;

void setUp() {}
void tearDown() {}

static bool inside(const Box& b, int w, int h) { return b.x >= 0 && b.y >= 0 && b.x + b.w <= w && b.y + b.h <= h && b.w > 0 && b.h > 0; }
static bool overlap(const Box& a, const Box& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void checkScreen(int w, int h)
{
    PfdLayout L = PfdLayout::compute(w, h);
    const Box* all[] = {&L.heading, &L.adi, &L.hour, &L.minute, &L.zulu, &L.local, &L.eicas, &L.maxg, &L.nm};
    for (const Box* b : all) {
        TEST_ASSERT_TRUE_MESSAGE(inside(*b, w, h), "box outside screen");
    }
    for (size_t i = 0; i < 9; i++) {
        for (size_t j = i + 1; j < 9; j++) {
            TEST_ASSERT_FALSE_MESSAGE(overlap(*all[i], *all[j]), "boxes overlap");
        }
    }
    TEST_ASSERT_EQUAL(L.adi.w, L.adi.h);                       // square
    TEST_ASSERT_INT_WITHIN(1, w / 2, L.adi.x + L.adi.w / 2);   // horizontally centred
}

// The T-Watch Ultra runs in portrait: 410 wide × 502 tall (LilyGoLib rotation 0).
static void test_ultra_portrait_410x502() { checkScreen(410, 502); }
static void test_landscape_502x410() { checkScreen(502, 410); }
static void test_s3_240x240() { checkScreen(240, 240); }

static void test_ultra_sizes()
{
    PfdLayout L = PfdLayout::compute(410, 502);
    TEST_ASSERT_TRUE(L.hour.h >= 24);
    TEST_ASSERT_TRUE(L.minute.h >= 24);
    TEST_ASSERT_TRUE(L.zulu.h >= 24);
    TEST_ASSERT_TRUE(L.local.h >= 24);
    TEST_ASSERT_TRUE(L.adi.w >= 230);   // readable instrument (capped near 238 px for FPS, ADR-011)
    TEST_ASSERT_TRUE(L.adi.w <= 240);
    TEST_ASSERT_TRUE(L.heading.w == L.adi.w);
    TEST_ASSERT_TRUE(L.zulu.w >= 120);  // room for "HH:MM" in the 28 px HUD font
    TEST_ASSERT_TRUE(L.hour.w >= 60);   // room for two 40 px HUD digits
}

static void test_ultra_tapes_flank_adi()
{
    PfdLayout L = PfdLayout::compute(410, 502);
    TEST_ASSERT_TRUE(L.hour.x + L.hour.w <= L.adi.x);
    TEST_ASSERT_TRUE(L.minute.x >= L.adi.x + L.adi.w);
    TEST_ASSERT_TRUE(L.heading.y + L.heading.h <= L.adi.y);
    TEST_ASSERT_TRUE(L.eicas.y >= L.adi.y + L.adi.h);
    TEST_ASSERT_TRUE(L.zulu.y >= L.eicas.y + L.eicas.h);   // portrait: time row below the EICAS
}

// The Ultra's AMOLED has rounded corners (r = 64 px): every PFD box must lie inside the visible area.
static void test_ultra_inside_rounded_corners()
{
    PfdLayout L = PfdLayout::compute(410, 502, 64);
    const Box* all[] = {&L.heading, &L.adi, &L.hour, &L.minute, &L.zulu, &L.local, &L.eicas, &L.maxg, &L.nm};
    for (const Box* b : all) {
        TEST_ASSERT_TRUE_MESSAGE(core::insideRounded(*b, 410, 502, 64), "box clipped by a rounded corner");
    }
}

static void test_inside_rounded_helper()
{
    TEST_ASSERT_TRUE(core::insideRounded(Box{100, 100, 50, 50}, 410, 502, 64));
    TEST_ASSERT_FALSE(core::insideRounded(Box{0, 0, 30, 30}, 410, 502, 64));      // top-left corner cut off
    TEST_ASSERT_FALSE(core::insideRounded(Box{380, 480, 30, 22}, 410, 502, 64));  // bottom-right corner cut off
    TEST_ASSERT_TRUE(core::insideRounded(Box{0, 0, 30, 30}, 410, 502, 0));        // square screen: fine
    TEST_ASSERT_TRUE(core::insideRounded(Box{64, 0, 50, 10}, 410, 502, 64));      // straight top edge
    // minTopForRounded: a box starting at the left edge must start at y >= r
    TEST_ASSERT_EQUAL(64, core::minTopForRounded(0, 50, 410, 64));
    TEST_ASSERT_EQUAL(0, core::minTopForRounded(64, 50, 410, 64));
}

static void test_ultra_clears_caution_banner()
{
    PfdLayout L = PfdLayout::compute(410, 502, 64);
    TEST_ASSERT_TRUE(L.heading.y >= 48);   // S0 caution banner is S(28) ≈ 48 px tall at the top
}

// Feature 011: KAAN emblem between ZULU and LOCAL (data-model PfdLayout.emblem).
static void test_ultra_emblem_box()
{
    const float aspect = float(kKaanEmblem.w) / kKaanEmblem.h;
    PfdLayout L = PfdLayout::compute(410, 502, 64, aspect);
    const Box& e = L.emblem;
    TEST_ASSERT_TRUE(e.w > 0 && e.h > 0);
    TEST_ASSERT_INT_WITHIN(1, 205, e.x + e.w / 2);                 // centred
    TEST_ASSERT_EQUAL(L.zulu.y, e.y);                              // top = the ZULU/LOCAL row
    const int m = 7;                                               // margin sc(4) at 410 px
    TEST_ASSERT_TRUE(e.y + e.h <= 502 - m);
    TEST_ASSERT_TRUE(core::insideRounded(e, 410, 502, 64));
    const Box* others[] = {&L.eicas, &L.adi, &L.hour, &L.minute, &L.heading, &L.maxg, &L.nm};
    for (const Box* b : others) {
        TEST_ASSERT_FALSE_MESSAGE(overlap(e, *b), "emblem overlaps a PFD box");
    }
    // the ZULU/LOCAL text ("88:88", DSEG7 28 px ≈ 100 px) is centred in its box: keep clear of centre ± 55 px
    const Box zt{L.zulu.x + L.zulu.w / 2 - 55, L.zulu.y, 110, L.zulu.h};
    const Box lt{L.local.x + L.local.w / 2 - 55, L.local.y, 110, L.local.h};
    TEST_ASSERT_FALSE(overlap(e, zt));
    TEST_ASSERT_FALSE(overlap(e, lt));
    const int contentW = L.minute.x + L.minute.w - L.hour.x;
    TEST_ASSERT_TRUE(e.w <= contentW / 4);
    // the generated emblem fits the box (±1 px)
    TEST_ASSERT_INT_WITHIN(1, kKaanEmblem.w, e.w);
    TEST_ASSERT_INT_WITHIN(1, kKaanEmblem.h, e.h);
}

static void test_emblem_absent_without_aspect_or_room()
{
    PfdLayout L = PfdLayout::compute(410, 502, 64);   // aspect 0: no emblem
    TEST_ASSERT_EQUAL(0, L.emblem.w);
    PfdLayout S = PfdLayout::compute(240, 240, 0, 0.67f);   // T-Watch S3: square layout, no gap
    TEST_ASSERT_EQUAL(0, S.emblem.w);
    TEST_ASSERT_EQUAL(0, S.emblem.h);
    PfdLayout Ls = PfdLayout::compute(502, 410, 0, 0.67f);  // landscape: none
    TEST_ASSERT_EQUAL(0, Ls.emblem.w);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_ultra_emblem_box);
    RUN_TEST(test_emblem_absent_without_aspect_or_room);
    RUN_TEST(test_ultra_portrait_410x502);
    RUN_TEST(test_landscape_502x410);
    RUN_TEST(test_s3_240x240);
    RUN_TEST(test_ultra_sizes);
    RUN_TEST(test_ultra_tapes_flank_adi);
    RUN_TEST(test_ultra_clears_caution_banner);
    RUN_TEST(test_ultra_inside_rounded_corners);
    RUN_TEST(test_inside_rounded_helper);
    return UNITY_END();
}
