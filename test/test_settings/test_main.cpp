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
    SettingDef secret{"wifipsk", SettingType::U16, 0, 0, 999, true, true};
    formatSettingForOutput(secret, 123, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("***", out);
    formatSettingForOutput(settingDef(SettingKey::Bright), 2, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("2", out);
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
    RUN_TEST(test_no_s0_setting_is_secret);
    return UNITY_END();
}
