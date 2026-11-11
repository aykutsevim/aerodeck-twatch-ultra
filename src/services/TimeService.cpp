#include "services/TimeService.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/time.h>
#include "board/IBoard.h"
#include "core/ClockMath.h"
#include "core/PosixTz.h"
#include "core/Zones.h"
#include "services/AlertManager.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"

namespace TimeService {

static Snapshot<ClockSnapshot> s_snap;
static volatile bool s_valid;
static uint32_t s_lastTickMs;

// Local offset from the selected named zone and its summer-time rules (S2, replaces the S1 fixed `tzoff`).
static core::PosixTz s_tz;
static int32_t s_tzId = -1;

static int16_t tzOffset()
{
    int32_t id = SettingsStore::get(core::SettingKey::TzId);
    if (id != s_tzId) {
        s_tzId = id;
        if (!s_tz.parse(core::Zones::at(size_t(id)).posix)) {
            s_tz.parse("UTC0");
        }
    }
    return s_tz.offsetMinutes(int64_t(time(nullptr)));
}

static void setSystemTime(int64_t utc)
{
    timeval tv{time_t(utc), 0};
    settimeofday(&tv, nullptr);
}

static void refresh()
{
    ClockSnapshot c;
    c.valid = s_valid;
    c.utc = int64_t(time(nullptr));
    c.tzOffsetMin = tzOffset();
    s_snap.write(c);
}

void begin()
{
    std::tm t{};
    bool integrity = false;
    s_valid = false;
    if (board().readRtc(t, integrity)) {
        int year = t.tm_year + 1900;
        int64_t utc = core::utcFromCivil(year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        s_valid = integrity && core::clockPlausible(year);
        if (s_valid) {
            setSystemTime(utc);
        }
        LOGI("time", "RTC %04d-%02d-%02dT%02d:%02d:%02dZ integrity=%d -> %s", year, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec, integrity ? 1 : 0, s_valid ? "valid" : "NOT SET");
    } else {
        LOGW("time", "RTC unavailable");
    }
    if (!s_valid) {
        AlertManager::raise(core::AlertId::ClockNotSet);
    }
    refresh();

    // S2: time zones (contracts/serial_commands.md)
    SerialConsole::add("tz", 0, [](int, const char* const*, core::Reply& r) {
        const size_t id = size_t(SettingsStore::get(core::SettingKey::TzId));
        const core::Zone& z = core::Zones::at(id);
        core::PosixTz tz;
        tz.parse(z.posix);
        const int64_t now = int64_t(time(nullptr));
        char b[96];
        snprintf(b, sizeof b, "tz=%s posix=%s offset_min=%d dst=%d", z.name, z.posix, tz.offsetMinutes(now),
                 tz.isDst(now) ? 1 : 0);
        r.ok();
        r.line(b);
    });
    SerialConsole::add("tz list", 0, [](int argc, const char* const* argv, core::Reply& r) {
        r.ok();
        char b[48];
        for (size_t i = 0; i < core::Zones::count(); i++) {
            const char* name = core::Zones::at(i).name;
            if (argc > 0) {   // case-insensitive substring filter
                bool hit = false;
                for (const char* p = name; *p && !hit; p++) {
                    const char* a = p;
                    const char* f = argv[0];
                    while (*a && *f && tolower(*a) == tolower(*f)) {
                        a++;
                        f++;
                    }
                    hit = (*f == '\0');
                }
                if (!hit) {
                    continue;
                }
            }
            snprintf(b, sizeof b, "%u %s", unsigned(i), name);
            r.line(b);
        }
    });
    SerialConsole::add("tz set", 1, [](int, const char* const* argv, core::Reply& r) {
        int id = core::Zones::find(argv[0]);
        if (id < 0) {
            char* end = nullptr;
            long v = strtol(argv[0], &end, 10);
            if (end && *end == '\0' && v >= 0 && size_t(v) < core::Zones::count()) {
                id = int(v);
            }
        }
        if (id < 0) {
            r.err("no such zone");
            return;
        }
        SettingsStore::set(core::SettingKey::TzId, id);
        refresh();   // FR-014: LOCAL follows within 1 s
        r.ok();
    });
    SerialConsole::add("time set", 1, [](int, const char* const* argv, core::Reply& r) {
        int64_t utc = 0;
        if (!core::parseIso8601Utc(argv[0], utc)) {
            r.err("bad time (expected YYYY-MM-DDTHH:MM:SSZ)");
            return;
        }
        if (!set(utc)) {
            r.err("rtc write failed");
            return;
        }
        r.ok();
    });
    SerialConsole::add("time", 0, [](int, const char* const*, core::Reply& r) {
        ClockSnapshot c = s_snap.read();
        int64_t utc = int64_t(time(nullptr));
        int y, mo, d, h, mi, s;
        core::civilFromUtc(utc, y, mo, d, h, mi, s);
        core::LocalTime l = core::splitLocal(utc, 0, c.tzOffsetMin);
        char b[96];
        snprintf(b, sizeof b, "utc=%04d-%02d-%02dT%02d:%02d:%02dZ valid=%d tzoff=%d local=%02u:%02u:%02u", y, mo, d,
                 h, mi, s, c.valid ? 1 : 0, c.tzOffsetMin, l.h, l.m, l.s);
        r.ok();
        r.line(b);
    });
}

const Snapshot<ClockSnapshot>& clock() { return s_snap; }

bool set(int64_t utc)
{
    int y, mo, d, h, mi, s;
    core::civilFromUtc(utc, y, mo, d, h, mi, s);
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    t.tm_wday = int((utc / 86400 + 4) % 7);   // 1970-01-01 was a Thursday
    if (!board().writeRtc(t)) {
        return false;
    }
    setSystemTime(utc);
    s_valid = core::clockPlausible(y);
    if (s_valid) {
        AlertManager::clear(core::AlertId::ClockNotSet);
    }
    refresh();
    EventBus::publish(EventType::ClockChanged);
    LOGI("time", "clock set to %04d-%02d-%02dT%02d:%02d:%02dZ", y, mo, d, h, mi, s);
    return true;
}

void tick(uint32_t now)
{
    if (now - s_lastTickMs < 1000) {
        return;
    }
    s_lastTickMs = now;
    refresh();
}

} // namespace TimeService
