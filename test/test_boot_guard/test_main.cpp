#include <unity.h>
#include "core/BootGuard.h"
#include "core/SimFail.h"

using namespace core;

void setUp() {}
void tearDown() {}

static BootRecord fresh()
{
    BootRecord r{};
    r.magic = 0xDEADBEEF;   // garbage, like uninitialised RTC memory
    r.fastFailCount = 200;
    return r;
}

static void test_invalid_magic_initialises()
{
    BootRecord r = fresh();
    BootMode m = BootGuard::onBoot(r, true);
    TEST_ASSERT_EQUAL_UINT32(BootGuard::kMagic, r.magic);
    TEST_ASSERT_EQUAL(0, r.fastFailCount);
    TEST_ASSERT_EQUAL(int(BootMode::Normal), int(m));
    TEST_ASSERT_EQUAL(int(BootStage::Start), int(r.lastStage));
}

static void boot(BootRecord& r, BootStage crashedAt)
{
    r.lastStage = crashedAt;   // what the previous boot reached before resetting
}

static void test_fast_fail_counts_and_enters_safe_mode()
{
    BootRecord r{};
    BootGuard::onBoot(r, true);   // init
    for (int i = 1; i <= 2; i++) {
        boot(r, BootStage::UiInit);
        TEST_ASSERT_EQUAL(int(BootMode::Normal), int(BootGuard::onBoot(r, true)));
        TEST_ASSERT_EQUAL(i, r.fastFailCount);
    }
    boot(r, BootStage::UiInit);
    TEST_ASSERT_EQUAL(int(BootMode::Safe), int(BootGuard::onBoot(r, true)));
    TEST_ASSERT_EQUAL(3, r.fastFailCount);
}

static void test_stable_clears_count()
{
    BootRecord r{};
    BootGuard::onBoot(r, true);
    boot(r, BootStage::UiInit);
    BootGuard::onBoot(r, true);
    TEST_ASSERT_EQUAL(1, r.fastFailCount);
    BootGuard::onStable(r);
    TEST_ASSERT_EQUAL(0, r.fastFailCount);
    TEST_ASSERT_EQUAL(int(BootStage::Stable), int(r.lastStage));
    // A reset after reaching stable is not a fast fail.
    TEST_ASSERT_EQUAL(int(BootMode::Normal), int(BootGuard::onBoot(r, true)));
    TEST_ASSERT_EQUAL(0, r.fastFailCount);
}

static void test_clean_reset_is_not_a_fast_fail()
{
    BootRecord r{};
    BootGuard::onBoot(r, true);
    boot(r, BootStage::UiInit);
    TEST_ASSERT_EQUAL(int(BootMode::Normal), int(BootGuard::onBoot(r, false)));   // power-on/sw reset
    TEST_ASSERT_EQUAL(0, r.fastFailCount);
}

static void test_display_stage_goes_headless()
{
    BootRecord r{};
    BootGuard::onBoot(r, true);
    for (int i = 0; i < 2; i++) {
        boot(r, BootStage::DisplayInit);
        BootGuard::onBoot(r, true);
    }
    boot(r, BootStage::DisplayInit);
    TEST_ASSERT_EQUAL(int(BootMode::Headless), int(BootGuard::onBoot(r, true)));
}

static void test_repeated_safe_mode_failure_goes_headless()
{
    BootRecord r{};
    BootGuard::onBoot(r, true);
    BootMode m = BootMode::Normal;
    for (int i = 0; i < BootGuard::kHeadlessAfter; i++) {
        boot(r, BootStage::VendorInit);
        m = BootGuard::onBoot(r, true);
    }
    TEST_ASSERT_EQUAL(int(BootMode::Headless), int(m));
}

static void test_simfail_mapping()
{
    TEST_ASSERT_EQUAL(0, SimFail::bitFor(Periph::Lora));
    TEST_ASSERT_EQUAL(7, SimFail::bitFor(Periph::Rtc));
    TEST_ASSERT_EQUAL(-1, SimFail::bitFor(Periph::Pmu));       // core peripherals cannot be simulated
    TEST_ASSERT_EQUAL(-1, SimFail::bitFor(Periph::Display));
    TEST_ASSERT_EQUAL(-1, SimFail::bitFor(Periph::Touch));
    TEST_ASSERT_EQUAL(int(Periph::Gnss), int(SimFail::periphFor(2)));
    TEST_ASSERT_EQUAL(int(Periph::Count), int(SimFail::periphFor(8)));
    TEST_ASSERT_EQUAL(3, SimFail::bitForName("sd"));
    TEST_ASSERT_EQUAL(-1, SimFail::bitForName("display"));
    TEST_ASSERT_EQUAL_STRING("haptics", SimFail::nameForBit(5));
    TEST_ASSERT_TRUE(SimFail::isSet(0x01, Periph::Lora));
    TEST_ASSERT_FALSE(SimFail::isSet(0x01, Periph::Nfc));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_invalid_magic_initialises);
    RUN_TEST(test_fast_fail_counts_and_enters_safe_mode);
    RUN_TEST(test_stable_clears_count);
    RUN_TEST(test_clean_reset_is_not_a_fast_fail);
    RUN_TEST(test_display_stage_goes_headless);
    RUN_TEST(test_repeated_safe_mode_failure_goes_headless);
    RUN_TEST(test_simfail_mapping);
    return UNITY_END();
}
