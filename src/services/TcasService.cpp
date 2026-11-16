#include "services/TcasService.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_attr.h>
#include <esp_random.h>
#include <sys/time.h>
#include "board/IBoard.h"
#include "core/BeaconCodec.h"
#include "core/DutyCycle.h"
#include "core/LoraAirtime.h"
#include "core/LoraRegion.h"
#include "services/AlertManager.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"

namespace TcasService {

using core::SettingKey;

namespace {

// ---- persistent duty history (RTC slow memory: survives eject and soft reset, research R4) ----
RTC_NOINIT_ATTR uint8_t s_dutyRtc[core::DutyCycle::kBlobMax];
RTC_NOINIT_ATTR uint32_t s_dutyRtcLen;

struct TxReq {
    uint8_t buf[core::BeaconCodec::kMaxSize];
    uint8_t n;
};
struct TxRes {
    bool ok;
    uint32_t airMs;
};

enum class TaskState : uint8_t { Off, Starting, Running, Failed, Stopping };

// svc-task state
core::DutyCycle s_duty;
core::TcasTable s_table;
Snapshot<TcasSnapshot> s_snap;
RailLease* s_lease;
core::LoraProfile s_prof;
uint8_t s_region, s_clamped;
bool s_probeDone, s_enabled, s_radioFailed;
uint32_t s_nextBeaconMs, s_dueMs, s_lastAgeMs, s_lastPubMs, s_lastCfgMs, s_seq;
bool s_txPending, s_delayLogged;
uint8_t s_beaconSeq;
uint16_t s_testIntervalS;
uint32_t s_sent, s_delayed, s_skipped, s_txFail, s_rx, s_foreign, s_crc, s_lastAir;
int32_t s_cfgPwr = -1, s_cfgSf = -1, s_cfgBw = -1, s_cfgReg = -1;
bool s_dirty = true;

// shared with the radio task
QueueHandle_t s_txQ, s_txResQ, s_rxQ;
volatile TaskState s_task = TaskState::Off;
volatile bool s_stopReq;
volatile bool s_probeBusy;
RadioProfile s_taskProf;

int64_t utcMs()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

RadioProfile toRadio(const core::LoraProfile& p)
{
    return RadioProfile{p.freqMHz, p.bwKHz, p.sf, p.crDenom, p.powerDbm, p.syncWord, p.preamble};
}

void callsign(char* out)   // 1..8 chars; empty setting → AD-xxxx from the node id (FR-005)
{
    SettingsStore::getStr(SettingKey::Callsign, out, 9);
    if (out[0] == '\0') {
        snprintf(out, 9, "AD-%04X", unsigned(board().nodeId() & 0xFFFF));
    }
}

uint16_t intervalS()
{
    return s_testIntervalS ? s_testIntervalS : uint16_t(SettingsStore::get(SettingKey::TcasInt));
}

uint32_t jitteredMs()
{
    const uint32_t base = uint32_t(intervalS()) * 1000u;
    const uint32_t span = base / 5;   // ±10 %
    return base - span / 2 + (span ? esp_random() % (span + 1) : 0);
}

uint32_t beaconAirMs() { return core::LoraAirtime::airtimeMs(s_prof.sf, s_prof.bwKHz, s_prof.crDenom, s_prof.preamble,
                                                             core::BeaconCodec::kSize, true, true); }

void saveDuty()
{
    s_dutyRtcLen = uint32_t(s_duty.save(s_dutyRtc, sizeof s_dutyRtc));
}

void loadProfile()
{
    s_cfgReg = SettingsStore::get(SettingKey::LoraReg);
    s_cfgPwr = SettingsStore::get(SettingKey::LoraPwr);
    s_cfgSf = SettingsStore::get(SettingKey::LoraSf);
    s_cfgBw = SettingsStore::get(SettingKey::LoraBw);
    s_region = uint8_t(s_cfgReg);
    s_prof = core::LoraRegion::resolve(s_region, s_cfgPwr, s_cfgSf, s_cfgBw, s_clamped);
    s_duty.setBudgetMs(core::LoraRegion::budgetMs(s_region));
    if (s_clamped) {
        LOGW("tcas", "settings clamped to %s limits (pwr %ld->%d, sf %ld->%u, bw idx %ld)",
             core::LoraRegion::limits(s_region).name, (long)s_cfgPwr, int(s_prof.powerDbm), (long)s_cfgSf,
             unsigned(s_prof.sf), (long)s_cfgBw);
    }
}

// ---- radio task (core 0) ----
void radioTask(void*)
{
    vTaskDelay(pdMS_TO_TICKS(20));   // rail settle (as in the deferred probe)
    if (!board().radioStart(s_taskProf)) {
        s_task = TaskState::Failed;
        vTaskDelete(nullptr);
        return;
    }
    s_task = TaskState::Running;
    while (!s_stopReq) {
        TxReq t;
        if (xQueueReceive(s_txQ, &t, 0) == pdTRUE) {
            TxRes r{};
            r.ok = board().radioSend(t.buf, t.n, r.airMs);
            xQueueSend(s_txResQ, &r, 0);
        }
        board().radioWait(50);
        RadioPacket p;
        while (board().radioPoll(p)) {
            xQueueSend(s_rxQ, &p, 0);
        }
    }
    board().radioStop();
    s_task = TaskState::Off;
    vTaskDelete(nullptr);
}

void startRadio(uint32_t now)
{
    loadProfile();
    s_lease = new RailLease(core::Rail::Lora);
    xQueueReset(s_txQ);
    xQueueReset(s_txResQ);
    xQueueReset(s_rxQ);
    s_taskProf = toRadio(s_prof);
    s_stopReq = false;
    s_task = TaskState::Starting;
    s_txPending = false;
    s_radioFailed = false;
    if (xTaskCreatePinnedToCore(radioTask, "radio", 4096, nullptr, 4, nullptr, 0) != pdPASS) {
        s_task = TaskState::Failed;
    }
    s_nextBeaconMs = now + 2000;   // first beacon shortly after start
    s_dueMs = s_nextBeaconMs;
    s_delayLogged = false;
    LOGI("tcas", "on (rail lora held) %s %.3f MHz SF%u BW%.0f %ddBm", core::LoraRegion::limits(s_region).name,
         s_prof.freqMHz, unsigned(s_prof.sf), s_prof.bwKHz, int(s_prof.powerDbm));
    s_dirty = true;
}

void stopRadio(const char* why)
{
    if (s_task == TaskState::Starting || s_task == TaskState::Running) {
        s_stopReq = true;
        const uint32_t t0 = millis();
        while (s_task != TaskState::Off && s_task != TaskState::Failed && millis() - t0 < 3000) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    s_task = TaskState::Off;
    delete s_lease;   // rail off when this was the last lease
    s_lease = nullptr;
    s_txPending = false;
    LOGI("tcas", "off (%s; rail lora released)", why);
    s_dirty = true;
}

bool radioRunning() { return s_task == TaskState::Running; }

void queueBeacon(uint32_t now, uint32_t air)
{
    core::Beacon b{};
    b.nodeId = board().nodeId();
    b.seq = s_beaconSeq++;
    callsign(b.callsign);
    const PowerSnapshot ps = PowerService::power().read();
    b.battPct = ps.valid ? ps.pmu.battPct : 0;
    TxReq t{};
    t.n = uint8_t(core::BeaconCodec::encode(b, t.buf));
    if (xQueueSend(s_txQ, &t, 0) == pdTRUE) {
        s_duty.record(utcMs(), air);   // counted when queued, at the computed airtime (conservative)
        saveDuty();
        s_txPending = true;
        LOGI("tcas", "beacon #%u %s batt=%u air %lu ms duty %lu/%lu ms", unsigned(b.seq), b.callsign,
             unsigned(b.battPct), (unsigned long)air, (unsigned long)s_duty.usedMs(utcMs()),
             (unsigned long)s_duty.budgetMs());
    }
    (void)now;
}

// Returns 0 when queued, otherwise the ms until the duty budget allows it.
int64_t tryBeacon(uint32_t now)
{
    const uint32_t air = beaconAirMs();
    if (!core::LoraRegion::dwellOk(s_region, air)) {
        return -1;
    }
    const int64_t u = utcMs();
    if (!s_duty.canSend(u, air)) {
        return s_duty.nextAllowedMs(u, air) - u;
    }
    queueBeacon(now, air);
    return 0;
}

void scheduleBeacon(uint32_t now)
{
    if (!radioRunning() || s_txPending || int32_t(now - s_nextBeaconMs) < 0) {
        return;
    }
    const int64_t wait = tryBeacon(now);
    if (wait == 0) {
        s_nextBeaconMs = now + jitteredMs();
        s_dueMs = s_nextBeaconMs;
        s_delayLogged = false;
        return;
    }
    if (wait < 0) {
        s_skipped++;
        LOGW("tcas", "beacon skipped: airtime %lu ms exceeds the %s dwell limit", (unsigned long)beaconAirMs(),
             core::LoraRegion::limits(s_region).name);
        s_nextBeaconMs = now + jitteredMs();
        s_dueMs = s_nextBeaconMs;
        return;
    }
    if (!s_delayLogged) {
        s_delayed++;
        s_delayLogged = true;
        LOGI("tcas", "beacon delayed by duty (%lld s)", (long long)((wait + 999) / 1000));
    }
    if (now - s_dueMs >= uint32_t(intervalS()) * 1000u) {   // more than one interval late: give this one up
        s_skipped++;
        LOGI("tcas", "beacon skipped (duty)");
        s_dueMs = now + jitteredMs();
        s_nextBeaconMs = s_dueMs;
        s_delayLogged = false;
        return;
    }
    const uint32_t step = wait > 1000 ? 1000u : uint32_t(wait);   // re-check at most every second
    s_nextBeaconMs = now + (step ? step : 1);
}

void onContact(const core::Beacon& b, int16_t rssi, int8_t snrQ4, uint32_t now, bool sim)
{
    const bool known = s_table.find(b.nodeId) >= 0;
    bool entered = false;
    const int i = s_table.update(b, rssi, snrQ4, now, sim, entered);
    const core::Contact& c = s_table.at(i);
    if (!known) {
        LOGI("tcas", "new contact %08lX %s rssi=%d ring=%u brg=%u%s", (unsigned long)c.nodeId, c.callsign, int(rssi),
             unsigned(c.ring), unsigned(c.bearingDeg), sim ? " sim" : "");
    }
    if (entered) {
        LOGI("tcas", "TRAFFIC %s", c.callsign);
        AlertManager::raise(core::AlertId::Traffic);   // the one advisory that speaks (FR-010)
    }
    s_dirty = true;
}

void drainRadio(uint32_t now)
{
    TxRes r;
    while (xQueueReceive(s_txResQ, &r, 0) == pdTRUE) {
        s_txPending = false;
        s_lastAir = r.airMs;
        if (r.ok) {
            s_sent++;
            LOGI("tcas", "beacon tx done air %lu ms (sent %lu)", (unsigned long)r.airMs, (unsigned long)s_sent);
        } else {
            s_txFail++;
            LOGW("tcas", "beacon tx failed (%d)", board().radioLastError());
        }
        s_dirty = true;
    }
    RadioPacket p;
    while (xQueueReceive(s_rxQ, &p, 0) == pdTRUE) {
        if (p.crcError) {
            s_crc++;
            continue;
        }
        core::Beacon b;
        if (!core::BeaconCodec::decode(p.data, p.len, b)) {
            s_foreign++;
            LOGI("tcas", "rx foreign len=%u rssi=%d", unsigned(p.len), int(p.rssi));
            continue;
        }
        if (b.nodeId == board().nodeId()) {
            continue;   // our own beacon (should not happen on half-duplex, but never show ourselves)
        }
        s_rx++;
        LOGI("tcas", "rx %08lX %s rssi=%d snr=%.1f", (unsigned long)b.nodeId, b.callsign, int(p.rssi),
             p.snrQ4 / 4.0f);
        onContact(b, p.rssi, p.snrQ4, now, false);
    }
}

void ageContacts(uint32_t now)
{
    const uint32_t timeout = uint32_t(SettingsStore::get(SettingKey::TcasAge)) * 1000u;
    for (int i = 0; i < s_table.count(); i++) {
        if (now - s_table.at(i).lastSeenMs >= timeout) {
            LOGI("tcas", "lost %s", s_table.at(i).callsign);
        }
    }
    if (s_table.age(now, timeout, nullptr, 0) > 0) {
        s_dirty = true;
    }
    if (!s_table.anyInner()) {
        AlertManager::clear(core::AlertId::Traffic);
    }
}

void publish(uint32_t now)
{
    TcasSnapshot s{};
    s.on = s_enabled;
    s.running = radioRunning();
    s.radioAvail = core::hasCap(board().caps(), core::Cap::Lora);
    s.radioOk = s.radioAvail && !s_radioFailed && board().periph(core::Periph::Lora) != core::PeriphState::Failed &&
                board().periph(core::Periph::Lora) != core::PeriphState::SimulatedFail;
    s.region = s_region;
    snprintf(s.regionName, sizeof s.regionName, "%s", core::LoraRegion::limits(s_region).name);
    s.freqMHz = s_prof.freqMHz;
    s.bwKHz = s_prof.bwKHz;
    s.powerDbm = s_prof.powerDbm;
    s.sf = s_prof.sf;
    callsign(s.callsign);
    s.nodeId = board().nodeId();
    s.intervalS = intervalS();
    s.sent = s_sent;
    s.delayed = s_delayed;
    s.skipped = s_skipped;
    s.txFail = s_txFail;
    s.rx = s_rx;
    s.foreign = s_foreign;
    s.crc = s_crc;
    s.dutyUsedMs = s_duty.usedMs(utcMs());
    s.dutyBudgetMs = s_duty.budgetMs();
    s.lastAirMs = s_lastAir;
    s.n = uint8_t(s_table.count());
    for (int i = 0; i < s_table.count(); i++) {
        s.contacts[i] = s_table.at(i);
    }
    s.atMs = now;
    s.seq = ++s_seq;
    s_snap.write(s);
}

// S7 pre-sleep hook: stop the radio cleanly (the board cuts POWER_RADIO in deep sleep anyway).
uint32_t sleepHook()
{
    if (s_task != TaskState::Off) {
        stopRadio("eject");
    }
    return 0;
}

// ---- console ----
bool parseHex(const char* s, uint32_t& out)
{
    char* e = nullptr;
    const unsigned long v = strtoul(s, &e, 16);
    if (!e || *e != '\0' || e == s) {
        return false;
    }
    out = uint32_t(v);
    return true;
}

void cmdTcas(int argc, const char* const* argv, core::Reply& r)
{
    const uint32_t now = millis();
    if (argc == 0) {
        publish(now);
        const TcasSnapshot s = s_snap.read();
        char b[320];
        snprintf(b, sizeof b,
                 "on=%d radio=%s region=%s freq=%.3f pwr=%d sf=%u bw=%.0f call=%s id=%08lX int=%u sent=%lu "
                 "delayed=%lu skipped=%lu txfail=%lu rx=%lu foreign=%lu crc=%lu duty=%.1fs/%.0fs contacts=%u",
                 s.on ? 1 : 0, !s.radioAvail ? "n/a" : !s.radioOk ? "fail" : s.running ? "ok" : "off", s.regionName,
                 s.freqMHz, int(s.powerDbm), unsigned(s.sf), s.bwKHz, s.callsign, (unsigned long)s.nodeId,
                 unsigned(s.intervalS), (unsigned long)s.sent, (unsigned long)s.delayed, (unsigned long)s.skipped,
                 (unsigned long)s.txFail, (unsigned long)s.rx, (unsigned long)s.foreign, (unsigned long)s.crc,
                 s.dutyUsedMs / 1000.0f, s.dutyBudgetMs / 1000.0f, unsigned(s.n));
        r.ok();
        r.line(b);
        for (int i = 0; i < s.n; i++) {
            const core::Contact& c = s.contacts[i];
            snprintf(b, sizeof b, "%08lX %s rssi=%d(ema %.1f) snr=%.1f ring=%u brg=%u age=%lus batt=%u sim=%d",
                     (unsigned long)c.nodeId, c.callsign, int(c.lastRssi), c.rssiEma, c.lastSnrQ4 / 4.0f,
                     unsigned(c.ring), unsigned(c.bearingDeg), (unsigned long)((now - c.lastSeenMs) / 1000),
                     unsigned(c.battPct), c.simulated ? 1 : 0);
            r.line(b);
        }
        return;
    }
    const char* v = argv[0];
    if (strcmp(v, "on") == 0 || strcmp(v, "off") == 0) {
        setEnabled(strcmp(v, "on") == 0);
        r.ok();
    } else if (strcmp(v, "beacon") == 0) {
        if (!radioRunning()) {
            r.err("radio not running (ad tcas on)");
            return;
        }
        if (s_txPending) {
            r.err("busy (a beacon is being sent)");
            return;
        }
        const int64_t wait = tryBeacon(now);
        char b[48];
        if (wait == 0) {
            snprintf(b, sizeof b, "OK queued air=%lums", (unsigned long)beaconAirMs());
            r.line(b);
        } else if (wait < 0) {
            r.err("dwell limit");
        } else {
            snprintf(b, sizeof b, "duty (next in %lld s)", (long long)((wait + 999) / 1000));
            r.err(b);
        }
    } else if (strcmp(v, "sim") == 0 && argc >= 4) {
        core::Beacon b{};
        if (!parseHex(argv[1], b.nodeId)) {
            r.err("id: hex");
            return;
        }
        int k = 0;
        for (const char* p = argv[2]; *p && k < 8; p++) {
            b.callsign[k++] = (*p >= 'a' && *p <= 'z') ? char(*p - 32) : *p;
        }
        b.callsign[k] = '\0';
        b.battPct = 50;
        const long rssi = strtol(argv[3], nullptr, 10);
        if (rssi < -140 || rssi > 0) {
            r.err("rssi -140..0");
            return;
        }
        onContact(b, int16_t(rssi), 32, now, true);   // SNR 8 dB
        const core::Contact& c = s_table.at(s_table.find(b.nodeId));
        char o[48];
        snprintf(o, sizeof o, "OK ring=%u ema=%.1f brg=%u", unsigned(c.ring), c.rssiEma, unsigned(c.bearingDeg));
        r.line(o);
    } else if (strcmp(v, "simclear") == 0) {
        s_table.removeSimulated();
        if (!s_table.anyInner()) {
            AlertManager::clear(core::AlertId::Traffic);
        }
        s_dirty = true;
        r.ok();
    } else if (strcmp(v, "interval") == 0 && argc >= 2) {
        const long s = strtol(argv[1], nullptr, 10);
        if (s != 0 && (s < 5 || s > 300)) {
            r.err("5..300 s (0 = the setting)");
            return;
        }
        s_testIntervalS = uint16_t(s);
        s_nextBeaconMs = now;
        s_dueMs = now;
        s_dirty = true;
        r.ok();
    } else {
        r.err("expected on|off|beacon|sim <id-hex> <call> <rssi>|simclear|interval <s>");
    }
}

// ---- S9 radio probe (T004 test hook): runs in its own task so the svc tick never blocks ----
struct ProbeArgs {
    bool tx;
    uint32_t seconds;
};
ProbeArgs s_probe;

void probeTask(void*)
{
    uint8_t cl = 0;
    core::LoraProfile lp = core::LoraRegion::resolve(0, 10, 9, 0, cl);   // EU868 default, always
    if (!s_probe.tx) {
        lp.syncWord = 0x34;   // public (LoRaWAN) sync word: listen only
    }
    {
        RailLease lease(core::Rail::Lora);
        vTaskDelay(pdMS_TO_TICKS(20));
        if (!board().radioStart(toRadio(lp))) {
            LOGW("probe", "s9 radio start failed (%d)", board().radioLastError());
        } else if (s_probe.tx) {
            uint8_t buf[core::BeaconCodec::kMaxSize];
            core::Beacon b{};
            b.nodeId = board().nodeId();
            snprintf(b.callsign, sizeof b.callsign, "PROBE");
            const uint32_t calc = core::LoraAirtime::airtimeMs(lp.sf, lp.bwKHz, lp.crDenom, lp.preamble,
                                                                core::BeaconCodec::kSize, true, true);
            for (int i = 0; i < 3; i++) {
                b.seq = uint8_t(i);
                const size_t n = core::BeaconCodec::encode(b, buf);
                uint32_t air = 0;
                const bool ok = board().radioSend(buf, n, air);
                LOGI("probe", "s9 tx %d %s air=%lu ms radiolib=%lu ms calc=%lu ms err=%d", i + 1,
                     ok ? "done" : "FAIL", (unsigned long)air, (unsigned long)board().radioAirtimeMs(n),
                     (unsigned long)calc, board().radioLastError());
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        } else {
            uint32_t pk = 0, crc = 0;
            int16_t lo = 0, hi = -200;
            const uint32_t t0 = millis();
            while (millis() - t0 < s_probe.seconds * 1000u) {
                board().radioWait(100);
                RadioPacket p;
                while (board().radioPoll(p)) {
                    if (p.crcError) {
                        crc++;
                    } else {
                        pk++;
                    }
                    lo = p.rssi < lo ? p.rssi : lo;
                    hi = p.rssi > hi ? p.rssi : hi;
                    LOGI("probe", "s9 rx len=%u rssi=%d snr=%.1f crc=%d", unsigned(p.len), int(p.rssi),
                         p.snrQ4 / 4.0f, p.crcError ? 1 : 0);
                }
            }
            LOGI("probe", "s9 rx done %lus packets=%lu crc=%lu rssi[%d..%d]", (unsigned long)s_probe.seconds,
                 (unsigned long)pk, (unsigned long)crc, int(pk + crc ? lo : 0), int(pk + crc ? hi : 0));
        }
        board().radioStop();
    }   // rail off
    LOGI("probe", "s9 done (rail lora released)");
    s_probeBusy = false;
    vTaskDelete(nullptr);
}

void cmdProbeS9(int argc, const char* const* argv, core::Reply& r)
{
    if (s_task != TaskState::Off || s_enabled) {
        r.err("turn TCAS off first (ad tcas off)");
        return;
    }
    if (s_probeBusy) {
        r.err("probe running");
        return;
    }
    if (strcmp(argv[0], "tx") == 0) {
        s_probe = {true, 0};
    } else if (strcmp(argv[0], "rx") == 0 && argc >= 2) {
        const long s = strtol(argv[1], nullptr, 10);
        if (s < 1 || s > 120) {
            r.err("seconds 1..120");
            return;
        }
        s_probe = {false, uint32_t(s)};
    } else {
        r.err("expected tx | rx <s>");
        return;
    }
    s_probeBusy = true;
    if (xTaskCreatePinnedToCore(probeTask, "s9probe", 4096, nullptr, 3, nullptr, 0) != pdPASS) {
        s_probeBusy = false;
        r.err("no task");
        return;
    }
    r.ok();
    r.line(s_probe.tx ? "probe: 3 beacons, EU868 869.525 MHz 10 dBm (see [probe] log)"
                      : "probe: listening, sync 0x34 (see [probe] log)");
}

} // namespace

void setEnabled(bool on) { SettingsStore::set(SettingKey::Tcas, on ? 1 : 0); }

const Snapshot<TcasSnapshot>& status() { return s_snap; }

void onProbeDone() { s_probeDone = true; }

void begin()
{
    s_txQ = xQueueCreate(2, sizeof(TxReq));
    s_txResQ = xQueueCreate(2, sizeof(TxRes));
    s_rxQ = xQueueCreate(8, sizeof(RadioPacket));
    if (s_dutyRtcLen > 0 && s_dutyRtcLen <= sizeof s_dutyRtc && s_duty.restore(s_dutyRtc, s_dutyRtcLen, utcMs())) {
        LOGI("tcas", "duty history restored: %lu ms in the last hour", (unsigned long)s_duty.usedMs(utcMs()));
    } else {
        s_dutyRtcLen = 0;
    }
    loadProfile();
    s_enabled = SettingsStore::getBool(SettingKey::Tcas);
    PowerService::addPreSleepHook(sleepHook);
    SerialConsole::add("tcas", 0, cmdTcas);
    SerialConsole::add("probe s9", 1, cmdProbeS9);
    publish(millis());
}

void tick(uint32_t now)
{
    // Desired state: the setting, after the deferred probe (it re-begins the radio at 434 MHz defaults), never
    // during a probe or an eject.
    s_enabled = SettingsStore::getBool(SettingKey::Tcas);
    const bool avail = core::hasCap(board().caps(), core::Cap::Lora);
    const bool want = s_enabled && avail && s_probeDone && !s_probeBusy && !PowerService::ejecting();
    if (want && s_task == TaskState::Off && !s_radioFailed) {
        startRadio(now);
    } else if (!want && s_task != TaskState::Off) {
        stopRadio(s_enabled ? "paused" : "setting");
    } else if (!s_enabled && s_radioFailed) {
        s_radioFailed = false;   // a later `ad tcas on` retries
    }
    if (s_task == TaskState::Failed) {
        LOGW("tcas", "radio start failed (%d)", board().radioLastError());
        delete s_lease;
        s_lease = nullptr;
        s_task = TaskState::Off;
        s_radioFailed = true;
        s_dirty = true;
    }
    // Radio settings changed while running: restart with the new profile (the duty history is kept).
    if (now - s_lastCfgMs >= 1000) {
        s_lastCfgMs = now;
        if (SettingsStore::get(SettingKey::LoraReg) != s_cfgReg || SettingsStore::get(SettingKey::LoraPwr) != s_cfgPwr ||
            SettingsStore::get(SettingKey::LoraSf) != s_cfgSf || SettingsStore::get(SettingKey::LoraBw) != s_cfgBw) {
            if (s_task != TaskState::Off) {
                stopRadio("reconfigure");   // restarted by the next tick
            } else {
                loadProfile();
            }
            s_dirty = true;
        }
    }
    drainRadio(now);
    scheduleBeacon(now);
    if (now - s_lastAgeMs >= 1000) {
        s_lastAgeMs = now;
        ageContacts(now);
    }
    if (s_dirty || now - s_lastPubMs >= 500) {   // 2 Hz for the page's ages
        s_dirty = false;
        s_lastPubMs = now;
        publish(now);
    }
}

} // namespace TcasService
