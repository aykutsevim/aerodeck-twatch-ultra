#include <unity.h>
#include <cstring>
#include <string>
#include "core/SettingDefs.h"
#include "core/Zones.h"

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

// S2 schema v3: tzoff replaced by tzid; Wi-Fi strings; sync interval; last sync.
static void test_schema_v3()
{
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(2, 3, true)));
    TEST_ASSERT_EQUAL(-1, findSettingKey("tzoff"));
}

static void test_wifi_strings()
{
    const SettingDef& s = settingDef(SettingKey::WifiSsid);
    TEST_ASSERT_EQUAL_STRING("wifissid", s.key);
    TEST_ASSERT_EQUAL(int(SettingType::Str), int(s.type));
    TEST_ASSERT_EQUAL(32, s.max);
    TEST_ASSERT_FALSE(s.editable);   // set via setup/serial, not the generic `ad set`
    TEST_ASSERT_FALSE(s.secret);
    const SettingDef& k = settingDef(SettingKey::WifiPsk);
    TEST_ASSERT_EQUAL_STRING("wifipsk", k.key);
    TEST_ASSERT_EQUAL(int(SettingType::Str), int(k.type));
    TEST_ASSERT_EQUAL(63, k.max);
    TEST_ASSERT_TRUE(k.secret);
    char out[16];
    formatSettingForOutput(k, 0, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("***", out);
}

static void test_validate_strings()
{
    TEST_ASSERT_TRUE(validateString(SettingKey::WifiSsid, "MyNet"));
    TEST_ASSERT_FALSE(validateString(SettingKey::WifiSsid, ""));
    TEST_ASSERT_FALSE(validateString(SettingKey::WifiSsid, "123456789012345678901234567890123"));   // 33
    TEST_ASSERT_TRUE(validateString(SettingKey::WifiPsk, "12345678"));
    TEST_ASSERT_FALSE(validateString(SettingKey::WifiPsk, "1234567"));   // < 8: WPA2/3 only
    TEST_ASSERT_FALSE(validateString(SettingKey::WifiPsk, ""));         // open networks unsupported
    std::string p63(63, 'x'), p64(64, 'x');
    TEST_ASSERT_TRUE(validateString(SettingKey::WifiPsk, p63.c_str()));
    TEST_ASSERT_FALSE(validateString(SettingKey::WifiPsk, p64.c_str()));
}

static void test_tzid_syncmin_lastsync()
{
    const SettingDef& t = settingDef(SettingKey::TzId);
    TEST_ASSERT_EQUAL_STRING("tzid", t.key);
    TEST_ASSERT_EQUAL(int(SettingType::U16), int(t.type));
    TEST_ASSERT_EQUAL(int(Zones::defaultIndex()), t.def);
    TEST_ASSERT_EQUAL(int(Zones::count()) - 1, t.max);
    TEST_ASSERT_TRUE(t.editable);
    const SettingDef& m = settingDef(SettingKey::SyncMin);
    TEST_ASSERT_EQUAL(60, m.def);
    TEST_ASSERT_EQUAL(15, m.min);
    TEST_ASSERT_EQUAL(1440, m.max);
    TEST_ASSERT_TRUE(m.editable);
    const SettingDef& l = settingDef(SettingKey::LastSync);
    TEST_ASSERT_EQUAL(int(SettingType::I32), int(l.type));
    TEST_ASSERT_FALSE(l.editable);
}

static void test_migrate_tzoff_to_tzid()
{
    TEST_ASSERT_EQUAL_STRING("Istanbul", Zones::at(migrateTzoffToTzid(180)).name);
    TEST_ASSERT_EQUAL_STRING("Kolkata", Zones::at(migrateTzoffToTzid(330)).name);
}

// S3 schema v4: weather station (additive).
static void test_schema_v4_wxstn()
{
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(3, 4, true)));
    const SettingDef& w = settingDef(SettingKey::WxStation);
    TEST_ASSERT_EQUAL_STRING("wxstn", w.key);
    TEST_ASSERT_EQUAL(int(SettingType::Str), int(w.type));
    TEST_ASSERT_EQUAL(4, w.min);
    TEST_ASSERT_EQUAL(4, w.max);
    TEST_ASSERT_TRUE(w.editable);
    TEST_ASSERT_FALSE(w.secret);
    TEST_ASSERT_NOT_NULL(w.sdef);
    TEST_ASSERT_EQUAL_STRING("LTAC", w.sdef);
    TEST_ASSERT_EQUAL(int(SettingKey::WxStation), findSettingKey("wxstn"));
    // Existing string settings have no string default (empty).
    TEST_ASSERT_NULL(settingDef(SettingKey::WifiSsid).sdef);
}

static void test_validate_station()
{
    TEST_ASSERT_TRUE(validateString(SettingKey::WxStation, "LTAC"));
    TEST_ASSERT_TRUE(validateString(SettingKey::WxStation, "ltfm"));   // case-insensitive
    TEST_ASSERT_FALSE(validateString(SettingKey::WxStation, "LTA"));
    TEST_ASSERT_FALSE(validateString(SettingKey::WxStation, "LTACX"));
    TEST_ASSERT_FALSE(validateString(SettingKey::WxStation, "LT1C"));
    TEST_ASSERT_FALSE(validateString(SettingKey::WxStation, ""));
    TEST_ASSERT_FALSE(validateString(SettingKey::WxStation, "LT C"));
    char v[8] = "ltfm";
    normalizeString(SettingKey::WxStation, v);
    TEST_ASSERT_EQUAL_STRING("LTFM", v);
    char ssid[8] = "MyNet";
    normalizeString(SettingKey::WifiSsid, ssid);   // other strings untouched
    TEST_ASSERT_EQUAL_STRING("MyNet", ssid);
}

// S4 schema v5: haptic level, quiet mode (additive).
static void test_schema_v5_haptics()
{
    TEST_ASSERT_TRUE(kSchemaVersion >= 5);
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(4, 5, true)));
    const SettingDef& h = settingDef(SettingKey::HapticLvl);
    TEST_ASSERT_EQUAL_STRING("hapticlvl", h.key);
    TEST_ASSERT_EQUAL(int(SettingType::U8), int(h.type));
    TEST_ASSERT_EQUAL(1, h.def);
    TEST_ASSERT_EQUAL(0, h.min);
    TEST_ASSERT_EQUAL(2, h.max);
    TEST_ASSERT_TRUE(h.editable);
    const SettingDef& q = settingDef(SettingKey::Quiet);
    TEST_ASSERT_EQUAL_STRING("quiet", q.key);
    TEST_ASSERT_EQUAL(int(SettingType::Bool), int(q.type));
    TEST_ASSERT_EQUAL(0, q.def);
    TEST_ASSERT_TRUE(q.editable);
    TEST_ASSERT_TRUE(validateSetting(SettingKey::HapticLvl, 2));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::HapticLvl, 3));
}

// S5 schema v6: volume, hourly chime, quiet hours (additive).
static void test_schema_v6_audio()
{
    TEST_ASSERT_EQUAL(6, kSchemaVersion);
    TEST_ASSERT_EQUAL(int(SchemaAction::Migrate), int(schemaAction(5, 6, true)));
    struct E { SettingKey k; const char* key; SettingType t; int32_t def, min, max; };
    const E e[4] = {
        {SettingKey::Volume, "volume", SettingType::U8, 3, 0, 4},
        {SettingKey::Chime, "chime", SettingType::Bool, 1, 0, 1},
        {SettingKey::QhStart, "qhstart", SettingType::U8, 22, 0, 23},
        {SettingKey::QhEnd, "qhend", SettingType::U8, 7, 0, 23},
    };
    for (const E& x : e) {
        const SettingDef& d = settingDef(x.k);
        TEST_ASSERT_EQUAL_STRING(x.key, d.key);
        TEST_ASSERT_EQUAL(int(x.t), int(d.type));
        TEST_ASSERT_EQUAL(x.def, d.def);
        TEST_ASSERT_EQUAL(x.min, d.min);
        TEST_ASSERT_EQUAL(x.max, d.max);
        TEST_ASSERT_TRUE(d.editable);
        TEST_ASSERT_FALSE(d.secret);
        TEST_ASSERT_EQUAL(int(x.k), findSettingKey(x.key));
        // additive migration: a missing key loads its default
        TEST_ASSERT_EQUAL(x.def, sanitizeSetting(x.k, 0, false));
    }
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Volume, 4));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::Volume, 5));
    TEST_ASSERT_FALSE(validateSetting(SettingKey::QhStart, 24));
    TEST_ASSERT_TRUE(validateSetting(SettingKey::QhEnd, 0));
}

static void test_s0_settings_have_no_step()
{
    TEST_ASSERT_EQUAL(0, settingDef(SettingKey::Bright).step);
    TEST_ASSERT_TRUE(validateSetting(SettingKey::Bright, 1));   // step 0 = any value in range
}

static void test_only_psk_is_secret()
{
    for (size_t i = 0; i < settingCount(); i++) {
        TEST_ASSERT_EQUAL(SettingKey(i) == SettingKey::WifiPsk, settingDef(SettingKey(i)).secret);
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
    RUN_TEST(test_schema_v3);
    RUN_TEST(test_wifi_strings);
    RUN_TEST(test_validate_strings);
    RUN_TEST(test_tzid_syncmin_lastsync);
    RUN_TEST(test_migrate_tzoff_to_tzid);
    RUN_TEST(test_schema_v4_wxstn);
    RUN_TEST(test_validate_station);
    RUN_TEST(test_schema_v5_haptics);
    RUN_TEST(test_schema_v6_audio);
    RUN_TEST(test_s0_settings_have_no_step);
    RUN_TEST(test_only_psk_is_secret);
    return UNITY_END();
}
