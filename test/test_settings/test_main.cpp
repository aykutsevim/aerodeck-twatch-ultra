#include <unity.h>
#include <cstring>
#include "core/SettingDefs.h"

using namespace core;

void setUp() {}
void tearDown() {}

static void test_registry_matches_data_model()
{
    TEST_ASSERT_EQUAL(size_t(SettingKey::Count), settingCount());
    const SettingDef& b = settingDef(SettingKey::Bright);
    TEST_ASSERT_EQUAL_STRING("bright", b.key);
    TEST_ASSERT_EQUAL(int(SettingType::U8), int(b.type));
    TEST_ASSERT_EQUAL(3, b.def);
    TEST_ASSERT_EQUAL(0, b.min);
    TEST_ASSERT_EQUAL(4, b.max);
    TEST_ASSERT_TRUE(b.editable);

    const SettingDef& o = settingDef(SettingKey::Overlay);
    TEST_ASSERT_EQUAL_STRING("overlay", o.key);
    TEST_ASSERT_EQUAL(int(SettingType::Bool), int(o.type));
    TEST_ASSERT_EQUAL(0, o.def);

    const SettingDef& l = settingDef(SettingKey::LogLvl);
    TEST_ASSERT_EQUAL_STRING("loglvl", l.key);
    TEST_ASSERT_EQUAL(2, l.def);
    TEST_ASSERT_EQUAL(3, l.max);

    const SettingDef& s = settingDef(SettingKey::SimFail);
    TEST_ASSERT_EQUAL_STRING("simfail", s.key);
    TEST_ASSERT_FALSE(s.editable);
    TEST_ASSERT_EQUAL(0, s.def);
    TEST_ASSERT_EQUAL(0xFF, s.max);
}

static void test_key_names_fit_nvs()
{
    for (size_t i = 0; i < settingCount(); i++) {
        TEST_ASSERT_TRUE(strlen(settingDef(SettingKey(i)).key) <= 15);
    }
    TEST_ASSERT_TRUE(strlen(kSchemaKey) <= 15);
}

static void test_validate()
{
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Bright, 0));
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Bright, 4));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::Bright, 5));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::Bright, -1));
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Overlay, 1));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::Overlay, 2));
}

static void test_sanitize()
{
    TEST_ASSERT_EQUAL(3, sanitizeSetting(SettingKey::Bright, 0, false));   // missing → default
    TEST_ASSERT_EQUAL(3, sanitizeSetting(SettingKey::Bright, 9, true));    // out of range → default
    TEST_ASSERT_EQUAL(1, sanitizeSetting(SettingKey::Bright, 1, true));    // valid → kept
}

static void test_schema_action()
{
    TEST_ASSERT_EQUAL(int(SchemaAction::FirstBoot), int(schemaAction(0, 1, false)));
    TEST_ASSERT_EQUAL(int(SchemaAction::Load), int(schemaAction(1, 1, true)));
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(1, 2, true)));
    TEST_ASSERT_EQUAL(int(SchemaAction::ResetNewer), int(schemaAction(3, 2, true)));
    TEST_ASSERT_EQUAL(int(SchemaAction::ResetNewer), int(schemaAction(0, 1, true)));   // corrupt 0
}

static void test_find_key()
{
    TEST_ASSERT_EQUAL(int(SettingKey::LogLvl), findSettingKey("loglvl"));
    TEST_ASSERT_EQUAL(-1, findSettingKey("nope"));
    TEST_ASSERT_EQUAL(-1, findSettingKey(nullptr));
}

static void test_format_masks_secret()
{
    char out[16];
    SettingDef secret{"wifipsk", SettingType::U16, 0, 0, 999, 0, true, true};
    formatSettingForOutput(secret, 123, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("***", out);
    formatSettingForOutput(settingDef(SettingKey::Bright), 2, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("2", out);
}

static void test_tzoff_s1()
{
    const SettingDef& t = settingDef(SettingKey::TzOff);
    TEST_ASSERT_EQUAL_STRING("tzoff", t.key);
    TEST_ASSERT_EQUAL(int(SettingType::I32), int(t.type));
    TEST_ASSERT_EQUAL(180, t.def);
    TEST_ASSERT_EQUAL(-720, t.min);
    TEST_ASSERT_EQUAL(840, t.max);
    TEST_ASSERT_EQUAL(30, t.step);
    TEST_ASSERT_TRUE(t.editable);
    TEST_ASSERT_TRUE(validateSetting(SettingKey::TzOff, 330));
    TEST_ASSERT_TRUE(validateSetting(SettingKey::TzOff, -720));
    TEST_ASSERT_TRUE(validateSetting(SettingKey::TzOff, 0));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::TzOff, 335));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::TzOff, -750));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::TzOff, 870));
}

static void test_schema_v2_migrates_v1()
{
    TEST_ASSERT_EQUAL(2, kSchemaVersion);
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(1, 2, true)));
}

static void test_s0_settings_have_no_step()
{
    TEST_ASSERT_EQUAL(0, settingDef(SettingKey::Bright).step);
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Bright, 1));   // step 0 = any value in range
}

static void test_no_s0_setting_is_secret()
{
    for (size_t i = 0; i < settingCount(); i++) {
        TEST_ASSERT_FALSE(settingDef(SettingKey(i)).secret);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_registry_matches_data_model);
    RUN_TEST(test_key_names_fit_nvs);
    RUN_TEST(test_validate);
    RUN_TEST(test_sanitize);
    RUN_TEST(test_schema_action);
    RUN_TEST(test_find_key);
    RUN_TEST(test_format_masks_secret);
    RUN_TEST(test_tzoff_s1);
    RUN_TEST(test_schema_v2_migrates_v1);
    RUN_TEST(test_s0_settings_have_no_step);
    RUN_TEST(test_no_s0_setting_is_secret);
    return UNITY_END();
}
