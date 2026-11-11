#include "services/SettingsStore.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/SerialConsole.h"

using namespace core;

namespace SettingsStore {

static int32_t s_cache[size_t(SettingKey::Count)];
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
    }
    return true;
}

static void writeKey(Preferences& p, const SettingDef& d, int32_t v)
{
    switch (d.type) {
    case SettingType::Bool: p.putBool(d.key, v != 0); break;
    case SettingType::U8: p.putUChar(d.key, uint8_t(v)); break;
    case SettingType::U16: p.putUShort(d.key, uint16_t(v)); break;
    case SettingType::I32: p.putInt(d.key, v); break;
    }
}

static void writeDefaults(Preferences& p)
{
    p.clear();
    p.putUShort(kSchemaKey, kSchemaVersion);
    for (size_t i = 0; i < settingCount(); i++) {
        const SettingDef& d = settingDef(SettingKey(i));
        writeKey(p, d, d.def);
        s_cache[i] = d.def;
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
        // No migrations exist yet (schema 1). Future: run stored+1..current steps here.
        LOGI("set", "migrating settings schema %u -> %u", stored, kSchemaVersion);
        p.putUShort(kSchemaKey, kSchemaVersion);
        status = BootStatus::Migrated;
        break;
    case SchemaAction::Load:
        break;
    }
    for (size_t i = 0; i < settingCount(); i++) {
        const SettingDef& d = settingDef(SettingKey(i));
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

void restoreDefaults()
{
    for (size_t i = 0; i < settingCount(); i++) {
        s_cache[i] = settingDef(SettingKey(i)).def;
    }
    uint8_t item = kRestoreAll;
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
            writeKey(p, d, s_cache[item]);
            LOGD("set", "saved '%s'", d.key);
        }
    } while (xQueueReceive(s_writes, &item, 0) == pdTRUE);
    p.end();
}

// ---- serial commands (contracts/serial_commands.md) ----

static void printSetting(core::Reply& r, size_t i)
{
    char v[24], line[48];
    const SettingDef& d = settingDef(SettingKey(i));
    formatSettingForOutput(d, s_cache[i], v, sizeof v);
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
    char* end = nullptr;
    long v = strtol(argv[1], &end, 0);
    const SettingDef& d = settingDef(SettingKey(k));
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
