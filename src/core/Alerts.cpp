#include "core/Alerts.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core {

namespace {

constexpr AlertDef kTable[] = {
    // id                          level                 pattern                repeat clip text                     name
    {AlertId::SettingsReset, AlertLevel::Caution, Pattern::Caution, 0, 0, "SETTINGS RESET", "settingsreset"},
    {AlertId::SettingsStorageError, AlertLevel::Caution, Pattern::Caution, 0, 0, "SETTINGS STORAGE ERROR", "settingsstorage"},
    {AlertId::SafeMode, AlertLevel::Caution, Pattern::Caution, 0, 0, "SAFE MODE", "safemode"},
    {AlertId::ClockNotSet, AlertLevel::Caution, Pattern::Caution, 0, 0, "CLOCK NOT SET", "clocknotset"},
    {AlertId::TimeUnsync, AlertLevel::Advisory, Pattern::Advisory, 0, 0, "TIME UNSYNC", "timeunsync"},
    {AlertId::Stall, AlertLevel::Warning, Pattern::StickShaker, 2000, uint8_t(Clip::Stall), "STALL", "stall"},
    {AlertId::PullUp, AlertLevel::Warning, Pattern::StickShaker, 2000, uint8_t(Clip::PullUp), "PULL UP", "pullup"},
    {AlertId::Test1, AlertLevel::Caution, Pattern::Caution, 0, uint8_t(Clip::Minimums), "TEST 1", "test1"},
    {AlertId::Test2, AlertLevel::Caution, Pattern::Caution, 0, uint8_t(Clip::Minimums), "TEST 2", "test2"},
    {AlertId::Test3, AlertLevel::Caution, Pattern::Caution, 0, uint8_t(Clip::Minimums), "TEST 3", "test3"},
};

// Shared definition for every peripheral id (text built from the peripheral name).
constexpr AlertDef kPeriph = {AlertId::PeriphFirst, AlertLevel::Caution, Pattern::Caution, 0, 0, nullptr, nullptr};

bool isPeriph(AlertId id) { return uint8_t(id) < uint8_t(Periph::Count); }

bool eqNoCase(const char* a, const char* b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = char(x + 32);
        if (y >= 'A' && y <= 'Z') y = char(y + 32);
        if (x != y) return false;
    }
    return *a == *b;
}

} // namespace

const AlertDef* alertDef(AlertId id)
{
    if (isPeriph(id)) {
        return &kPeriph;
    }
    for (const AlertDef& d : kTable) {
        if (d.id == id) {
            return &d;
        }
    }
    return nullptr;
}

void alertText(AlertId id, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    if (isPeriph(id)) {
        snprintf(out, n, "%s FAIL", periphName(Periph(uint8_t(id))));
        return;
    }
    const AlertDef* d = alertDef(id);
    snprintf(out, n, "%s", d && d->text ? d->text : "ALERT");
}

bool alertIdFromName(const char* name, AlertId& out)
{
    if (!name || !*name) {
        return false;
    }
    bool digits = true;
    for (const char* p = name; *p; p++) {
        if (*p < '0' || *p > '9') {
            digits = false;
        }
    }
    if (digits) {
        const long v = strtol(name, nullptr, 10);
        if (v < 0 || v > 255 || !alertDef(AlertId(uint8_t(v)))) {
            return false;
        }
        out = AlertId(uint8_t(v));
        return true;
    }
    for (const AlertDef& d : kTable) {
        if (eqNoCase(name, d.name)) {
            out = d.id;
            return true;
        }
    }
    for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
        if (eqNoCase(name, periphName(Periph(i)))) {
            out = AlertId(i);
            return true;
        }
    }
    return false;
}

bool levelFromLetter(const char* s, AlertLevel& out)
{
    if (!s || !s[0] || s[1]) {
        return false;
    }
    switch (s[0]) {
    case 'W': case 'w': out = AlertLevel::Warning; return true;
    case 'C': case 'c': out = AlertLevel::Caution; return true;
    case 'A': case 'a': out = AlertLevel::Advisory; return true;
    default: return false;
    }
}

const char* levelName(AlertLevel l)
{
    switch (l) {
    case AlertLevel::Warning: return "WARNING";
    case AlertLevel::Caution: return "CAUTION";
    case AlertLevel::Advisory: return "ADVISORY";
    default: return "NONE";
    }
}

Pattern patternForLevel(AlertLevel l)
{
    switch (l) {
    case AlertLevel::Warning: return Pattern::StickShaker;
    case AlertLevel::Caution: return Pattern::Caution;
    case AlertLevel::Advisory: return Pattern::Advisory;
    default: return Pattern::None;
    }
}

namespace {
constexpr const char* kClipNames[] = {"master_caution", "master_warning", "stall", "pull_up",
                                      "bingo_fuel", "minimums", "traffic", "chime"};
constexpr const char* kClipFiles[] = {"master_caution.wav", "master_warning.wav", "stall.wav", "pull_up.wav",
                                      "bingo_fuel.wav", "minimums.wav", "traffic.wav", "chime.wav"};
static_assert(sizeof(kClipNames) / sizeof(kClipNames[0]) == size_t(Clip::Count) - 1, "clip table");
} // namespace

const char* clipName(Clip c)
{
    return (c == Clip::None || uint8_t(c) >= uint8_t(Clip::Count)) ? nullptr : kClipNames[uint8_t(c) - 1];
}

const char* clipFile(Clip c)
{
    return (c == Clip::None || uint8_t(c) >= uint8_t(Clip::Count)) ? nullptr : kClipFiles[uint8_t(c) - 1];
}

bool clipFromName(const char* name, Clip& out)
{
    if (!name || !*name) {
        return false;
    }
    for (uint8_t i = 1; i < uint8_t(Clip::Count); i++) {
        if (eqNoCase(name, kClipNames[i - 1])) {
            out = Clip(i);
            return true;
        }
    }
    return false;
}

Clip calloutFor(AlertId id, AlertLevel level)
{
    if (level != AlertLevel::Caution && level != AlertLevel::Warning) {
        return Clip::None;   // Advisory (and None) never speak
    }
    const AlertDef* d = alertDef(id);
    if (d && d->voiceClip != 0 && d->voiceClip < uint8_t(Clip::Count)) {
        return Clip(d->voiceClip);
    }
    return level == AlertLevel::Warning ? Clip::MasterWarning : Clip::MasterCaution;
}

} // namespace core
