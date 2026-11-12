#include "services/RecorderService.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_system.h>
#include "board/IBoard.h"
#include "core/FlightMath.h"
#include "core/RecBlob.h"
#include "core/SettingDefs.h"
#include "core/StepAccumulator.h"
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/TimeService.h"

using core::DayLog;
using core::DayRecord;
using core::SettingKey;

namespace RecorderService {

namespace {

constexpr const char* kNs = "rec";
constexpr const char* kKey = "day";
constexpr uint32_t kPeriodMs = 1000;
constexpr uint32_t kSaveEveryMs = 10u * 60u * 1000u;   // when changed (research R4)
constexpr uint32_t kAccelSettleMs = 2000;   // the hub reports a ~2 g transient right after the accel starts (T012)

DayLog s_log;                        // svc task only (the console runs on the svc task too)
core::StepAccumulator s_acc;
Snapshot<RecorderSnapshot> s_snap;
RecorderSnapshot s_tmp;
MotionInfo s_motion{false, false, 0.0f, 0.0f};
volatile bool s_motionPending;       // set by onImuReady (deferred-probe task), consumed by tick
float s_gNow;
bool s_dirty;
uint32_t s_lastTickMs;
uint32_t s_accelFromMs;            // peaks before this are discarded (start-up transient)
uint32_t s_lastSaveMs;
uint32_t s_saves;
uint32_t s_rejected;
uint32_t s_seq;
bool s_begun;

struct Clock {
    bool valid;
    int32_t day;
    int minute;
};

Clock clockNow()
{
    const ClockSnapshot c = TimeService::clock().read();
    if (!c.valid) {
        return Clock{false, core::kUndated, -1};
    }
    return Clock{true, DayLog::localDayOf(c.utc, c.tzOffsetMin), DayLog::minuteOfDay(c.utc, c.tzOffsetMin)};
}

void fmtDate(int32_t day, char* out, size_t n)
{
    if (day == core::kUndated) {
        snprintf(out, n, "undated");
        return;
    }
    if (day == core::kEmptyDay) {
        snprintf(out, n, "-");
        return;
    }
    int y, m, d;
    DayLog::civil(day, y, m, d);
    snprintf(out, n, "%04d-%02d-%02d", y, m, d);
}

uint8_t stride() { return uint8_t(SettingsStore::get(SettingKey::Stride)); }

void publish()
{
    s_tmp.stepsOk = s_motion.steps;
    s_tmp.accelOk = s_motion.accel;
    s_tmp.gNow = s_gNow;
    s_tmp.steps = s_log.today.steps;
    s_tmp.maxGc = s_log.today.maxGc;
    s_tmp.maxGMin = s_log.today.maxGMin;
    s_tmp.rangeG = s_motion.rangeG;
    s_tmp.maxGSat = s_motion.rangeG > 0.0f && s_log.today.maxGc >= uint16_t(s_motion.rangeG * 100.0f);
    s_tmp.strideCm = stride();
    for (int i = 0; i < 7; i++) {
        s_tmp.chart[i] = s_log.dayAt(i);
    }
    s_tmp.today = s_log.today.day;
    s_tmp.lastSaveMs = s_lastSaveMs;
    s_tmp.saves = s_saves;
    s_tmp.rejectedJumps = s_rejected;
    s_tmp.seq = ++s_seq;
    s_snap.write(s_tmp);
}

bool writeBlob()
{
    uint8_t buf[core::RecBlob::kSize];
    core::RecBlob::encode(s_log, buf);
    Preferences p;
    if (!p.begin(kNs, false)) {
        return false;
    }
    const bool ok = p.putBytes(kKey, buf, sizeof buf) == sizeof buf;
    p.end();
    return ok;
}

// Shutdown handler (inside esp_restart): write only, no logging.
void onShutdown()
{
    if (s_begun && s_dirty) {
        writeBlob();
    }
}

void roll(const core::RollResult& r, int32_t oldDay)
{
    char a[12], b[12];
    if (r.dated) {
        fmtDate(s_log.today.day, a, sizeof a);
        LOGI("rec", "dated %s", a);
        s_dirty = true;
    }
    if (r.rolled) {
        fmtDate(oldDay, a, sizeof a);
        fmtDate(s_log.today.day, b, sizeof b);
        LOGI("rec", "rollover %s -> %s (empty=%u)", a, b, unsigned(r.emptyDays));
        s_dirty = true;
        publish();   // show the new day at once (FR-006)
        saveNow("rollover");
    }
}

// ---- serial commands (contracts/serial_commands.md) ----

void dayLine(const char* tag, const DayRecord& d, uint8_t strideCm, core::Reply& r)
{
    char date[12], nm[12], g[12], b[112];
    fmtDate(d.day, date, sizeof date);
    static const char* kWd[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const char* wd = (d.day == core::kUndated || d.day == core::kEmptyDay) ? "---" : kWd[DayLog::weekday(d.day)];
    if (d.steps == 0 && d.maxGc == 0) {
        snprintf(b, sizeof b, "%s %s %s empty", tag, date, wd);
    } else {
        core::FlightMath::formatNm(core::FlightMath::nm(d.steps, strideCm), true, nm, sizeof nm);
        core::FlightMath::formatG(d.maxGc, d.maxGc > 0, false, g, sizeof g);
        if (d.maxGMin >= 0) {
            snprintf(b, sizeof b, "%s %s %s steps=%lu nm=%s maxg=%s@%02d:%02d", tag, date, wd, (unsigned long)d.steps,
                     nm, g, d.maxGMin / 60, d.maxGMin % 60);
        } else {
            snprintf(b, sizeof b, "%s %s %s steps=%lu nm=%s maxg=%s", tag, date, wd, (unsigned long)d.steps, nm, g);
        }
    }
    r.line(b);
}

void cmdStatus(int, const char* const*, core::Reply& r)
{
    const RecorderSnapshot s = s_snap.read();
    char date[12], nm[12], g[12], b[200];
    fmtDate(s.today, date, sizeof date);
    core::FlightMath::formatNm(core::FlightMath::nm(s.steps, s.strideCm), s.stepsOk, nm, sizeof nm);
    core::FlightMath::formatG(s.maxGc, s.accelOk, s.maxGSat, g, sizeof g);
    char at[8] = "";
    if (s.accelOk && s.maxGMin >= 0) {
        snprintf(at, sizeof at, "@%02d:%02d", s.maxGMin / 60, s.maxGMin % 60);
    }
    snprintf(b, sizeof b,
             "today=%s steps=%lu nm=%s maxg=%s%s stride=%u steps_src=%s accel=%s g_now=%.2f range=%.0f saves=%lu "
             "last_save=%lus rejected=%lu",
             date, (unsigned long)s.steps, nm, g, at, unsigned(s.strideCm), s.stepsOk ? "OK" : "absent",
             s.accelOk ? "OK" : "absent", double(s.gNow), double(s.rangeG), (unsigned long)s.saves,
             (unsigned long)(s.saves ? (millis() - s.lastSaveMs) / 1000u : 0u), (unsigned long)s.rejectedJumps);
    r.ok();
    r.line(b);
    for (int i = 1; i < 7; i++) {
        char tag[6];
        snprintf(tag, sizeof tag, "d-%d", i);
        dayLine(tag, s.chart[i], s.strideCm, r);
    }
    char tag[6] = "d-7";
    dayLine(tag, s_log.history[6], s.strideCm, r);   // the 7th stored day (not drawn on the watch)
}

void cmdSave(int, const char* const*, core::Reply& r)
{
    if (saveNow("cmd")) {
        r.line("OK saved");
    } else {
        r.err("nvs");
    }
}

void cmdSimSteps(int, const char* const* argv, core::Reply& r)
{
    const long n = strtol(argv[0], nullptr, 10);
    if (n < 1 || n > 100000) {
        r.err("expected 1..100000");
        return;
    }
    s_log.addSteps(uint32_t(n));
    s_dirty = true;
    publish();
    r.ok();
}

void cmdSimG(int, const char* const* argv, core::Reply& r)
{
    const float g = strtof(argv[0], nullptr);
    if (!(g > 0.0f) || g > 100.0f) {
        r.err("expected 0.1..100");
        return;
    }
    bool sat = false;
    const uint16_t gc = core::FlightMath::toCentiG(g, s_motion.rangeG, sat);
    const Clock c = clockNow();
    if (s_log.notePeak(gc, int16_t(c.minute))) {
        s_dirty = true;
        LOGI("rec", "max g %.1f at %02d:%02d (sim)", double(gc) / 100.0, c.minute / 60, c.minute % 60);
        publish();
        r.ok();
        return;
    }
    char b[48], cur[12];
    core::FlightMath::formatG(s_log.today.maxGc, true, false, cur, sizeof cur);
    snprintf(b, sizeof b, "OK lower (max stays %s)", cur);
    r.line(b);
}

void cmdRollover(int, const char* const*, core::Reply& r)
{
    const Clock c = clockNow();
    const int32_t base = s_log.today.day == core::kUndated ? (c.valid ? c.day : 0) : s_log.today.day;
    if (s_log.today.day == core::kUndated) {
        s_log.advance(true, base);
    }
    const int32_t old = s_log.today.day;
    roll(s_log.advance(true, base + 1), old);
    r.line("OK rolled");
}

void cmdClear(int, const char* const*, core::Reply& r)
{
    s_log.clear();
    const Clock c = clockNow();
    if (c.valid) {
        s_log.advance(true, c.day);
    }
    Preferences p;
    if (p.begin(kNs, false)) {
        p.remove(kKey);
        p.end();
    }
    s_dirty = false;
    LOGI("rec", "cleared");
    publish();
    r.ok();
}

} // namespace

void begin()
{
    Preferences p;
    uint8_t buf[core::RecBlob::kSize + 4];
    size_t n = 0;
    if (p.begin(kNs, true)) {
        n = p.isKey(kKey) ? p.getBytes(kKey, buf, sizeof buf) : 0;
        p.end();
    }
    char date[12];
    if (n == 0) {
        LOGI("rec", "start: no saved data");
    } else if (!core::RecBlob::decode(buf, n, s_log)) {
        s_log.clear();
        LOGW("rec", "load failed (format/crc, %u bytes): starting empty", unsigned(n));
    } else {
        int hist = 0;
        for (const DayRecord& d : s_log.history) {
            hist += d.day != core::kEmptyDay ? 1 : 0;
        }
        fmtDate(s_log.today.day, date, sizeof date);
        LOGI("rec", "start today=%s steps=%lu hist=%d", date, (unsigned long)s_log.today.steps, hist);
    }
    s_lastSaveMs = millis();
    publish();
    esp_register_shutdown_handler(onShutdown);
    s_begun = true;
    SerialConsole::add("rec", 0, cmdStatus);
    SerialConsole::add("rec save", 0, cmdSave);
    SerialConsole::add("rec sim steps", 1, cmdSimSteps);
    SerialConsole::add("rec sim g", 1, cmdSimG);
    SerialConsole::add("rec rollover", 0, cmdRollover);
    SerialConsole::add("rec clear", 0, cmdClear);
}

void onImuReady(bool imuOk)
{
    MotionInfo m{false, false, 0.0f, 0.0f};
    if (imuOk) {
        board().configureMotion(50.0f, m);   // FR-REC-002: ≥50 Hz
    }
    s_motion = m;
    s_motionPending = true;
    LOGI("rec", "motion steps=%s accel=%s scale=%.6f range=%.0f", m.steps ? "OK" : "absent",
         m.accel ? "OK" : "absent", double(m.accelScale), double(m.rangeG));
}

void tick(uint32_t now)
{
    if (now - s_lastTickMs < kPeriodMs) {
        return;
    }
    s_lastTickMs = now;
    if (s_motionPending) {
        s_motionPending = false;
        s_acc.reset(now);   // the hub's step count starts at 0 when it is (re)started
        s_accelFromMs = now + kAccelSettleMs;
    }

    uint32_t raw;
    if (s_motion.steps && board().stepCount(raw)) {
        bool rejected = false;
        const uint32_t last = s_acc.last();
        const uint32_t d = s_acc.feed(raw, now, rejected);
        if (rejected) {
            s_rejected++;
            LOGW("rec", "step jump rejected raw=%lu last=%lu", (unsigned long)raw, (unsigned long)last);
        } else if (d) {
            s_log.addSteps(d);
            s_dirty = true;
        }
    }

    const Clock c = clockNow();
    float peak, last, range;
    if (s_motion.accel && board().takePeakG(peak, last, range)) {
        s_gNow = last;
        if (int32_t(now - s_accelFromMs) < 0) {
            peak = 0.0f;   // drained, not recorded
        }
        bool sat = false;
        const uint16_t gc = core::FlightMath::toCentiG(peak, range, sat);
        const uint16_t before = s_log.today.maxGc;
        if (s_log.notePeak(gc, int16_t(c.minute))) {
            s_dirty = true;
            if (gc >= before + 10) {   // log rises of ≥0.1 g only (keeps the log quiet)
                LOGI("rec", "max g %.1f at %02d:%02d", double(gc) / 100.0, c.minute / 60, c.minute % 60);
            }
        }
    }

    const int32_t oldDay = s_log.today.day;
    roll(s_log.advance(c.valid, c.day), oldDay);

    if (s_dirty && now - s_lastSaveMs >= kSaveEveryMs) {
        saveNow("periodic");
    }
    publish();
}

const Snapshot<RecorderSnapshot>& status() { return s_snap; }

bool saveNow(const char* reason)
{
    const uint32_t t0 = millis();
    const bool ok = writeBlob();
    if (ok) {
        s_dirty = false;
        s_saves++;
        s_lastSaveMs = millis();
        LOGI("rec", "saved (%s) %lu ms", reason, (unsigned long)(millis() - t0));
    } else {
        LOGE("rec", "save failed (%s)", reason);
    }
    return ok;
}

} // namespace RecorderService
