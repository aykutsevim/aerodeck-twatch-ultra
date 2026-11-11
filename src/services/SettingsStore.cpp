#include "services/SettingsStore.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstdlib>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/SerialConsole.h"

using namespace core;

namespace SettingsStore {

static int32_t s_cache[size_t(SettingKey::Count)];
static constexpr size_t kStrMax = 64;   // longest string setting (psk 63) + NUL
static char s_str[size_t(SettingKey::Count)][kStrMax];   // used only for SettingType::Str
static portMUX_TYPE s_strLock = portMUX_INITIALIZER_UNLOCKED;
static bool s_storageOk;
static QueueHandle_t s_writes;   // items: SettingKey index, or 0xFF = restore defaults
static constexpr uint8_t kRestoreAll = 0xFF;

static bool readKey(Preferences& p, const SettingDef& d, int32_t& out)
{
    if (!p.isKey(d.key)) {
        return false;
    }
    switch (d.type) {
    case SettingType::Bool: out = p.getBool(d.key) ? 1 : 0; break;
    case SettingType::U8: out = p.getUChar(d.key); break;
    case SettingType::U16: out = p.getUShort(d.key); break;
    case SettingType::I32: out = p.getInt(d.key); break;
    case SettingType::Str: out = 0; break;   // strings are read with readStr
    }
    return true;
}

static const char* strDefault(const SettingDef& d) { return d.sdef ? d.sdef : ""; }

// Returns true when the key was present in NVS.
static bool readStr(Preferences& p, size_t i, const SettingDef& d)
{
    char buf[kStrMax] = {0};
    const bool has = p.isKey(d.key);
    if (has) {
        p.getString(d.key, buf, sizeof buf);
    } else {
        strncpy(buf, strDefault(d), kStrMax - 1);
    }
    if (buf[0] != '\0' && !validateString(SettingKey(i), buf)) {
        LOGW("set", "setting '%s' invalid; using default", d.key);   // key only, never value
        strncpy(buf, strDefault(d), kStrMax - 1);
    }
    normalizeString(SettingKey(i), buf);
    memcpy(s_str[i], buf, kStrMax);
    return has;
}

static void writeKey(Preferences& p, const SettingDef& d, int32_t v)
{
    switch (d.type) {
    case SettingType::Bool: p.putBool(d.key, v != 0); break;
    case SettingType::U8: p.putUChar(d.key, uint8_t(v)); break;
    case SettingType::U16: p.putUShort(d.key, uint16_t(v)); break;
    case SettingType::I32: p.putInt(d.key, v); break;
    case SettingType::Str: break;   // written by writeStr
    }
}

static void writeStr(Preferences& p, size_t i, const SettingDef& d)
{
    char buf[kStrMax];
    portENTER_CRITICAL(&s_strLock);
    memcpy(buf, s_str[i], kStrMax);
    portEXIT_CRITICAL(&s_strLock);
    if (buf[0] == '\0') {
        p.remove(d.key);
    } else {
        p.putString(d.key, buf);
    }
}

static void writeDefaults(Preferences& p)
{
    p.clear();
    p.putUShort(kSchemaKey, kSchemaVersion);
    for (size_t i = 0; i < settingCount(); i++) {
        const SettingDef& d = settingDef(SettingKey(i));
        s_cache[i] = d.def;
        if (d.type == SettingType::Str) {
            // restore defaults also forgets the Wi-Fi network; wxstn returns to LTAC
            strncpy(s_str[i], strDefault(d), kStrMax - 1);
            s_str[i][kStrMax - 1] = '\0';
            if (s_str[i][0] != '\0') {
                p.putString(d.key, s_str[i]);
            }
            continue;
        }
        writeKey(p, d, d.def);
    }
}

BootStatus begin()
{
    s_writes = xQueueCreate(16, sizeof(uint8_t));
    for (size_t i = 0; i < settingCount(); i++) {
        s_cache[i] = settingDef(SettingKey(i)).def;
    }
    Preferences p;
    if (!p.begin(kNamespace, false)) {
        LOGE("set", "NVS open failed; using defaults");
        s_storageOk = false;
        return BootStatus::StorageError;
    }
    s_storageOk = true;
    bool present = p.isKey(kSchemaKey);
    uint16_t stored = present ? p.getUShort(kSchemaKey, 0) : 0;
    BootStatus status = BootStatus::Loaded;
    switch (schemaAction(stored, kSchemaVersion, present)) {
    case SchemaAction::FirstBoot:
        LOGI("set", "first boot: writing defaults");
        writeDefaults(p);
        p.end();
        return BootStatus::FirstBoot;
    case SchemaAction::ResetNewer:
        LOGW("set", "unknown settings schema %u (current %u): reset to defaults", stored, kSchemaVersion);
        writeDefaults(p);
        p.end();
        return BootStatus::ResetNewer;
    case SchemaAction::Migrate:
        LOGI("set", "migrating settings schema %u -> %u", stored, kSchemaVersion);
        if (stored < 3 && p.isKey(kLegacyTzoffKey)) {
            // 2→3 (S2): the S1 fixed offset becomes an equivalent named/fixed zone, so LOCAL doesn't change.
            int32_t tzoff = p.getInt(kLegacyTzoffKey, 180);
            uint16_t tzid = migrateTzoffToTzid(tzoff);
            p.putUShort(settingDef(SettingKey::TzId).key, tzid);
            p.remove(kLegacyTzoffKey);
            LOGI("set", "migration: tzoff %ld -> tzid %u", (long)tzoff, tzid);
        }
        p.putUShort(kSchemaKey, kSchemaVersion);
        status = BootStatus::Migrated;
        break;
    case SchemaAction::Load:
        break;
    }
    for (size_t i = 0; i < settingCount(); i++) {
        const SettingDef& d = settingDef(SettingKey(i));
        if (d.type == SettingType::Str) {
            if (!readStr(p, i, d) && status == BootStatus::Migrated && d.sdef) {
                p.putString(d.key, s_str[i]);   // key added by the new schema (v4 wxstn): persist its default
                LOGI("set", "migration added '%s'", d.key);
            }
            continue;
        }
        int32_t v = 0;
        bool has = readKey(p, d, v);
        s_cache[i] = sanitizeSetting(SettingKey(i), v, has);
        if (!has && status == BootStatus::Migrated) {
            writeKey(p, d, s_cache[i]);   // key added by the new schema: persist its default
            LOGI("set", "migration added '%s'", d.key);
        }
        if (has && s_cache[i] != v) {
            LOGW("set", "setting '%s' out of range; using default", d.key);   // key only, never value
        }
    }
    p.end();
    return status;
}

int32_t get(SettingKey k)
{
    size_t i = size_t(k);
    return i < settingCount() ? s_cache[i] : 0;
}

SetResult set(SettingKey k, int32_t v, bool internal)
{
    const SettingDef& d = settingDef(k);
    if (!internal && !d.editable) {
        return SetResult::NotEditable;
    }
    if (!validateSetting(k, v)) {
        return SetResult::OutOfRange;
    }
    if (s_cache[size_t(k)] != v) {
        s_cache[size_t(k)] = v;
        uint8_t item = uint8_t(k);
        if (s_writes) {
            xQueueSend(s_writes, &item, 0);
        }
        EventBus::publish(EventType::SettingsChanged, uint32_t(k));
    }
    return SetResult::Ok;
}

void getStr(SettingKey k, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    size_t i = size_t(k);
    portENTER_CRITICAL(&s_strLock);
    strncpy(out, i < settingCount() ? s_str[i] : "", n - 1);
    portEXIT_CRITICAL(&s_strLock);
    out[n - 1] = '\0';
}

static void storeStr(SettingKey k, const char* v)
{
    size_t i = size_t(k);
    portENTER_CRITICAL(&s_strLock);
    strncpy(s_str[i], v, kStrMax - 1);
    s_str[i][kStrMax - 1] = '\0';
    portEXIT_CRITICAL(&s_strLock);
    uint8_t item = uint8_t(k);
    if (s_writes) {
        xQueueSend(s_writes, &item, 0);
    }
    EventBus::publish(EventType::SettingsChanged, uint32_t(k));
}

SetResult setStr(SettingKey k, const char* v)
{
    if (settingDef(k).type != SettingType::Str) {
        return SetResult::NotEditable;
    }
    if (!validateString(k, v)) {
        return SetResult::OutOfRange;
    }
    char buf[kStrMax];
    strncpy(buf, v, kStrMax - 1);
    buf[kStrMax - 1] = '\0';
    normalizeString(k, buf);
    storeStr(k, buf);
    return SetResult::Ok;
}

void clearStr(SettingKey k)
{
    if (settingDef(k).type == SettingType::Str) {
        storeStr(k, "");
    }
}

void restoreDefaults()
{
    for (size_t i = 0; i < settingCount(); i++) {
        const SettingDef& d = settingDef(SettingKey(i));
        s_cache[i] = d.def;
        portENTER_CRITICAL(&s_strLock);
        strncpy(s_str[i], strDefault(d), kStrMax - 1);
        s_str[i][kStrMax - 1] = '\0';
        portEXIT_CRITICAL(&s_strLock);
    }
    uint8_t item = kRestoreAll;   // writeDefaults() clears the namespace, including blobs (wxcache)
    if (s_writes) {
        xQueueSend(s_writes, &item, 0);
    }
    for (size_t i = 0; i < settingCount(); i++) {
        EventBus::publish(EventType::SettingsChanged, uint32_t(i));
    }
}

void tick(uint32_t)
{
    if (!s_writes || !s_storageOk) {
        return;
    }
    uint8_t item;
    if (xQueueReceive(s_writes, &item, 0) != pdTRUE) {
        return;
    }
    Preferences p;
    if (!p.begin(kNamespace, false)) {
        LOGE("set", "NVS open failed during write");
        return;
    }
    do {
        if (item == kRestoreAll) {
            writeDefaults(p);
            LOGI("set", "defaults restored");
        } else if (item < settingCount()) {
            const SettingDef& d = settingDef(SettingKey(item));
            if (d.type == SettingType::Str) {
                writeStr(p, item, d);
            } else {
                writeKey(p, d, s_cache[item]);
            }
            LOGD("set", "saved '%s'", d.key);   // key only, never value
        }
    } while (xQueueReceive(s_writes, &item, 0) == pdTRUE);
    p.end();
}

// ---- blobs: internal cached data, not settings (S3 research R7) ----

static bool blobKeyOk(const char* key)
{
    return key && key[0] && strlen(key) <= 15 && findSettingKey(key) < 0 && strcmp(key, kSchemaKey) != 0;
}

size_t getBlob(const char* key, void* buf, size_t cap)
{
    if (!blobKeyOk(key) || !buf || !s_storageOk) {
        return 0;
    }
    Preferences p;
    if (!p.begin(kNamespace, true)) {
        return 0;
    }
    size_t n = p.isKey(key) ? p.getBytesLength(key) : 0;
    if (n == 0 || n > cap) {
        p.end();
        return 0;
    }
    n = p.getBytes(key, buf, n);
    p.end();
    return n;
}

bool setBlob(const char* key, const void* data, size_t len)
{
    if (!blobKeyOk(key) || !data || len == 0 || !s_storageOk) {
        return false;
    }
    Preferences p;
    if (!p.begin(kNamespace, false)) {
        return false;
    }
    const bool ok = p.putBytes(key, data, len) == len;
    p.end();
    return ok;
}

void eraseBlob(const char* key)
{
    if (!blobKeyOk(key) || !s_storageOk) {
        return;
    }
    Preferences p;
    if (p.begin(kNamespace, false)) {
        if (p.isKey(key)) {
            p.remove(key);
        }
        p.end();
    }
}

// ---- serial commands (contracts/serial_commands.md) ----

static void printSetting(core::Reply& r, size_t i)
{
    char v[kStrMax], line[kStrMax + 24];
    const SettingDef& d = settingDef(SettingKey(i));
    if (d.type == SettingType::Str && !d.secret) {
        getStr(SettingKey(i), v, sizeof v);
    } else {
        formatSettingForOutput(d, s_cache[i], v, sizeof v);   // secret → "***"
    }
    snprintf(line, sizeof line, "%s=%s", d.key, v);
    r.line(line);
}

static void cmdList(int, const char* const*, core::Reply& r)
{
    r.ok();
    for (size_t i = 0; i < settingCount(); i++) {
        printSetting(r, i);
    }
}

static void cmdGet(int, const char* const* argv, core::Reply& r)
{
    int k = findSettingKey(argv[0]);
    if (k < 0) {
        r.err("no such key");
        return;
    }
    r.ok();
    printSetting(r, size_t(k));
}

static void cmdSet(int, const char* const* argv, core::Reply& r)
{
    int k = findSettingKey(argv[0]);
    if (k < 0) {
        r.err("no such key");
        return;
    }
    const SettingDef& d = settingDef(SettingKey(k));
    if (d.type == SettingType::Str) {
        if (!d.editable) {
            r.err("not editable");
        } else if (setStr(SettingKey(k), argv[1]) != SetResult::Ok) {
            r.err("invalid");
        } else {
            r.ok();
        }
        return;
    }
    char* end = nullptr;
    long v = strtol(argv[1], &end, 0);
    char msg[48];
    if (!end || *end != '\0') {
        if (d.step > 0) {
            snprintf(msg, sizeof msg, "out of range [%ld..%ld step %ld]", (long)d.min, (long)d.max, (long)d.step);
        } else {
            snprintf(msg, sizeof msg, "out of range [%ld..%ld]", (long)d.min, (long)d.max);
        }
        r.err(msg);
        return;
    }
    switch (set(SettingKey(k), int32_t(v))) {
    case SetResult::Ok: r.ok(); break;
    case SetResult::NotEditable: r.err("not editable"); break;
    case SetResult::OutOfRange:
        if (d.step > 0) {
            snprintf(msg, sizeof msg, "out of range [%ld..%ld step %ld]", (long)d.min, (long)d.max, (long)d.step);
        } else {
            snprintf(msg, sizeof msg, "out of range [%ld..%ld]", (long)d.min, (long)d.max);
        }
        r.err(msg);
        break;
    }
}

static void cmdReset(int, const char* const*, core::Reply& r)
{
    restoreDefaults();
    r.ok();
}

void registerCommands()
{
    SerialConsole::add("set list", 0, cmdList);
    SerialConsole::add("set get", 1, cmdGet);
    SerialConsole::add("set reset", 0, cmdReset);
    SerialConsole::add("set", 2, cmdSet);
}

} // namespace SettingsStore
