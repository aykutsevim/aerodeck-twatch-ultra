#include "services/ChronoService.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include "core/ChronoBlob.h"
#include "services/AlertManager.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"

namespace ChronoService {

using core::ChronoModel;
using core::SettingKey;
using M = ChronoModel;

namespace {

constexpr const char* kNs = "chrono";
constexpr const char* kKey = "st";
constexpr int64_t kLeadMs = 5000;       // wake this long before the event: the boot takes ~3.4 s (research R3)
constexpr int64_t kPostponeMs = 6000;   // closer than this: do not eject (the alert fires awake)

struct CmdMsg {
    Cmd c;
    uint32_t a, b;
};

ChronoModel s_model;
QueueHandle_t s_q;
Snapshot<ChronoSnapshot> s_snap;
uint32_t s_seq;
uint32_t s_saves;
uint32_t s_lastPubMs;
uint32_t s_lastCfgMs;
bool s_dirty = true;
int32_t s_lastErrMs = INT32_MIN;   // device-side error of the last expiry (test hook output)
bool s_testCfg;   // a test hook set the lengths: do not reload them from the settings until reset

int64_t nowMs()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

const char* stateName(M::State s)
{
    switch (s) {
    case M::State::Running: return "running";
    case M::State::Paused: return "paused";
    case M::State::Expired: return "expired";
    default: return "idle";
    }
}

const char* phaseName(M::Phase p)
{
    switch (p) {
    case M::Phase::Work: return "work";
    case M::Phase::Break: return "break";
    default: return "countdown";
    }
}

void fmtMs(char* b, size_t n, int64_t ms)
{
    if (ms < 0) {
        ms = 0;
    }
    snprintf(b, n, "%02ld:%02ld.%03ld", long(ms / 60000), long((ms / 1000) % 60), long(ms % 1000));
}

void configureFromSettings()
{
    if (s_testCfg) {
        return;
    }
    M::Config c{SettingsStore::getBool(SettingKey::ChrMode) ? M::Mode::Pomodoro : M::Mode::Countdown,
                int64_t(SettingsStore::get(SettingKey::ChrLen)) * 60000,
                int64_t(SettingsStore::get(SettingKey::ChrWork)) * 60000,
                int64_t(SettingsStore::get(SettingKey::ChrBrk)) * 60000};
    s_model.configure(c);
}

void save()
{
    uint8_t buf[core::ChronoBlob::kSize];
    core::ChronoBlob::encode(s_model.raw(), buf);
    Preferences p;
    if (p.begin(kNs, false)) {
        p.putBytes(kKey, buf, sizeof buf);
        p.end();
        s_saves++;
        LOGD("chrono", "saved (%lu)", (unsigned long)s_saves);
    }
    s_dirty = true;
}

void load()
{
    uint8_t buf[core::ChronoBlob::kSize];
    size_t n = 0;
    Preferences p;
    if (p.begin(kNs, true)) {
        n = p.isKey(kKey) ? p.getBytes(kKey, buf, sizeof buf) : 0;
        p.end();
    }
    ChronoModel::Raw r;
    if (n == sizeof buf && core::ChronoBlob::decode(buf, n, r)) {
        s_model.load(r);
    } else if (n) {
        LOGW("chrono", "saved state rejected; starting idle");
    }
}

void clearAlerts()
{
    AlertManager::clear(core::AlertId::BingoFuel);
    AlertManager::clear(core::AlertId::BreakOver);
}

void handle(M::Event e, int64_t now)
{
    switch (e) {
    case M::Event::Expired: {
        const int64_t err = now - s_model.raw().endUtcMs;
        s_lastErrMs = int32_t(err);
        LOGW("chrono", "expired at %lld (err %lld ms)%s", (long long)s_model.raw().endUtcMs, (long long)err,
             s_model.late() ? " LATE" : "");
        if (s_model.late()) {
            AlertManager::raise(core::AlertId::BingoFuel, core::AlertLevel::Caution, "BINGO FUEL (LATE)");
        } else {
            AlertManager::raise(core::AlertId::BingoFuel);
        }
        save();
        break;
    }
    case M::Event::WorkDone:
        LOGW("chrono", "work done (cycle %u)%s", unsigned(s_model.cycles()), s_model.late() ? " LATE" : "");
        AlertManager::clear(core::AlertId::BreakOver);
        AlertManager::raise(core::AlertId::BingoFuel);
        save();
        break;
    case M::Event::BreakDone:
        LOGW("chrono", "break done%s", s_model.late() ? " LATE" : "");
        AlertManager::clear(core::AlertId::BingoFuel);
        AlertManager::raise(core::AlertId::BreakOver);
        save();
        break;
    default:
        break;
    }
}

void exec(const CmdMsg& m, int64_t now)
{
    char t[16];
    switch (m.c) {
    case Cmd::Toggle:
        if (s_model.state() == M::State::Running) {
            exec(CmdMsg{Cmd::Pause, 0, 0}, now);
        } else {
            exec(CmdMsg{Cmd::Start, 0, 0}, now);
        }
        return;
    case Cmd::Start:
        if (s_model.state() != M::State::Paused) {
            clearAlerts();
            configureFromSettings();
        }
        s_model.start(now);
        fmtMs(t, sizeof t, s_model.remainingMs(now));
        LOGI("chrono", "start %s mode=%s", t, s_model.mode() == M::Mode::Pomodoro ? "pomodoro" : "countdown");
        break;
    case Cmd::Pause:
        s_model.pause(now);
        fmtMs(t, sizeof t, s_model.remainingMs(now));
        LOGI("chrono", "pause rem=%s", t);
        break;
    case Cmd::Resume:
        s_model.resume(now);
        LOGI("chrono", "resume");
        break;
    case Cmd::Reset:
        s_model.reset();
        s_testCfg = false;
        configureFromSettings();
        clearAlerts();
        LOGI("chrono", "reset");
        break;
    case Cmd::StartTest:
    case Cmd::PomoTest: {
        s_model.reset();
        clearAlerts();
        const M::Config c = m.c == Cmd::StartTest
                                ? M::Config{M::Mode::Countdown, int64_t(m.a) * 1000, 60000, 60000}
                                : M::Config{M::Mode::Pomodoro, 60000, int64_t(m.a) * 1000, int64_t(m.b) * 1000};
        s_model.configure(c);
        s_testCfg = true;
        s_model.start(now);
        LOGI("chrono", "start (test) %lu s%s", (unsigned long)m.a, m.c == Cmd::PomoTest ? " pomodoro" : "");
        break;
    }
    case Cmd::Reconfigure:
        configureFromSettings();
        s_dirty = true;
        return;   // settings only: nothing to persist in the blob
    }
    save();
}

void publish(int64_t now)
{
    ChronoSnapshot s;
    s.state = s_model.state();
    s.mode = s_model.mode();
    s.phase = s_model.phase();
    s.remainingMs = int32_t(s_model.remainingMs(now));
    s.intervalMs = int32_t(s_model.intervalMs());
    s.fraction = s_model.fraction(now);
    s.cycles = s_model.cycles();
    s.late = s_model.late();
    s.endUtcMs = s_model.nextEventUtcMs();
    s.saves = s_saves;
    s.seq = ++s_seq;
    s_snap.write(s);
}

// S7 pre-sleep hook (data-model §6): runs on the svc task inside PowerService::eject().
uint32_t sleepHook()
{
    const int64_t next = s_model.nextEventUtcMs();
    if (next < 0) {
        return 0;
    }
    const int64_t r = next - nowMs();
    if (r <= kPostponeMs) {
        LOGI("chrono", "sleep hook: postpone (%lld ms left)", (long long)r);
        return PowerService::kPostponeSleep;
    }
    int64_t s = (r - kLeadMs) / 1000;
    if (s < 1) {
        s = 1;
    }
    LOGI("chrono", "sleep hook: wake_in=%lld s", (long long)s);
    return uint32_t(s);
}

void cmdChrono(int argc, const char* const* argv, core::Reply& r)
{
    if (argc == 0) {
        const ChronoSnapshot s = s_snap.read();
        char rem[16], iv[16], b[256];
        fmtMs(rem, sizeof rem, s.remainingMs);
        fmtMs(iv, sizeof iv, s.intervalMs);
        snprintf(b, sizeof b,
                 "mode=%s state=%s phase=%s rem=%s interval=%s frac=%.2f cycles=%u late=%d end_utc=%lld saves=%lu last_err_ms=%ld",
                 s.mode == M::Mode::Pomodoro ? "pomodoro" : "countdown", stateName(s.state), phaseName(s.phase),
                 rem, iv, s.fraction, unsigned(s.cycles), s.late ? 1 : 0, (long long)s.endUtcMs,
                 (unsigned long)s.saves, long(s_lastErrMs));
        r.ok();
        r.line(b);
        return;
    }
    const char* v = argv[0];
    if (strcmp(v, "start") == 0) {
        if (argc >= 2) {
            const long sec = strtol(argv[1], nullptr, 10);
            if (sec < 1 || sec > 7200) {
                r.err("seconds 1..7200");
                return;
            }
            post(Cmd::StartTest, uint32_t(sec));
        } else {
            post(Cmd::Start);
        }
    } else if (strcmp(v, "pause") == 0) {
        if (s_snap.read().state != M::State::Running) {
            r.err("not running");
            return;
        }
        post(Cmd::Pause);
    } else if (strcmp(v, "resume") == 0) {
        if (s_snap.read().state != M::State::Paused) {
            r.err("not paused");
            return;
        }
        post(Cmd::Resume);
    } else if (strcmp(v, "reset") == 0) {
        post(Cmd::Reset);
    } else if (strcmp(v, "pomo") == 0 && argc >= 3) {
        const long w = strtol(argv[1], nullptr, 10), b = strtol(argv[2], nullptr, 10);
        if (w < 1 || w > 7200 || b < 1 || b > 7200) {
            r.err("seconds 1..7200");
            return;
        }
        post(Cmd::PomoTest, uint32_t(w), uint32_t(b));
    } else {
        r.err("expected start [s]|pause|resume|reset|pomo <workS> <breakS>");
        return;
    }
    r.ok();
}

} // namespace

void post(Cmd c, uint32_t a, uint32_t b)
{
    if (s_q) {
        CmdMsg m{c, a, b};
        xQueueSend(s_q, &m, 0);
    }
}

const Snapshot<ChronoSnapshot>& status() { return s_snap; }

void begin()
{
    s_q = xQueueCreate(8, sizeof(CmdMsg));
    load();
    const int64_t now = nowMs();
    if (s_model.state() == M::State::Idle) {
        configureFromSettings();
    }
    const M::Event e = s_model.restore(now);
    char t[16];
    fmtMs(t, sizeof t, s_model.remainingMs(now));
    LOGI("chrono", "restored %s rem=%s%s", stateName(s_model.state()), t, s_model.late() ? " late" : "");
    if (e != M::Event::None) {
        handle(e, now);
    }
    PowerService::addPreSleepHook(sleepHook);
    SerialConsole::add("chrono", 0, cmdChrono);
    publish(now);
}

void tick(uint32_t ms)
{
    const int64_t now = nowMs();
    CmdMsg m;
    while (s_q && xQueueReceive(s_q, &m, 0) == pdTRUE) {
        exec(m, now);
    }
    const M::Event e = s_model.tick(now);
    if (e != M::Event::None) {
        handle(e, now);
    }
    if (s_model.state() != M::State::Running && ms - s_lastCfgMs >= 1000) {   // idle: pick up settings changes
        s_lastCfgMs = ms;
        if (s_model.state() == M::State::Idle) {
            const int64_t before = s_model.intervalMs();
            const M::Mode modeBefore = s_model.mode();
            configureFromSettings();
            if (s_model.intervalMs() != before || s_model.mode() != modeBefore) {
                s_dirty = true;
            }
        }
    }
    // Snapshot: 5 Hz while running (MM:SS + tank), otherwise only on change (FR-015: idle costs nothing).
    if (s_dirty || (s_model.state() == M::State::Running && ms - s_lastPubMs >= 200)) {
        s_dirty = false;
        s_lastPubMs = ms;
        publish(now);
    }
}

} // namespace ChronoService
