#include "services/WeatherService.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include <time.h>
#include "core/HttpResp.h"
#include "core/SettingDefs.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/NetService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/TimeService.h"
#include "services/WxCerts.h"

#ifndef AD_FW_VERSION
#define AD_FW_VERSION "dev"
#endif

using core::SettingKey;
namespace Metar = core::Metar;

namespace WeatherService {

namespace {

constexpr const char* kHost = "aviationweather.gov";
constexpr uint16_t kPort = 443;
constexpr const char* kCacheKey = "wxcache";
constexpr uint8_t kCacheVersion = 1;
constexpr size_t kRxCap = core::HttpResp::kMaxHeader + size_t(core::HttpResp::kMaxBody) + 4;

Snapshot<WeatherSnapshot> s_snap;
WeatherSnapshot s_st;                 // guarded by s_lock (net task + svc task + serial)
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
uint32_t s_seq;
char* s_rx;                           // PSRAM receive buffer (kRxCap + 1)
QueueHandle_t s_events;
volatile int64_t s_ageOverrideS = -1;

// ---- size-based mbedTLS allocator (research R3) ----
// Large blocks (TLS record buffers, handshake) go to PSRAM so internal heap stays >= 40 KB; small blocks keep
// the default internal placement. Installed once, never swapped: the Wi-Fi supplicant also uses mbedTLS.
void* wxCalloc(size_t n, size_t size)
{
    if (size != 0 && n > SIZE_MAX / size) {
        return nullptr;
    }
    if (n * size >= 4096) {
        void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) {
            return p;
        }
    }
    return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void wxFree(void* p) { heap_caps_free(p); }

struct __attribute__((packed)) CacheRec {
    uint8_t version;
    char station[5];
    int64_t fetchedUtc;
    int64_t obsUtc;
    uint16_t rawLen;
    char raw[Metar::kMaxRaw];
};

int64_t nowUtc()
{
    ClockSnapshot c = TimeService::clock().read();
    return c.valid ? c.utc : 0;
}

void publishLocked()   // call with s_lock held; copies out
{
    s_st.seq = ++s_seq;
    s_st.ageOverrideS = s_ageOverrideS;
}

// Two writers (net task, svc task): the snapshot write happens inside the lock so Snapshot<T> stays single-writer.
void publish()
{
    portENTER_CRITICAL(&s_lock);
    publishLocked();
    s_snap.write(s_st);
    portEXIT_CRITICAL(&s_lock);
}

void currentStation(char out[5])
{
    char b[8] = {0};
    SettingsStore::getStr(SettingKey::WxStation, b, sizeof b);
    memcpy(out, b, 4);
    out[4] = 0;
}

// Replace the report from a raw line (decode + resolve obs time). Caller holds no lock.
bool loadRaw(const char* raw, int64_t fetchedUtc, bool fromCache)
{
    static Metar::Report r;   // net/svc task only, never concurrent with itself (s_lock below)
    if (!Metar::decode(raw, r) || r.nil) {
        return false;
    }
    const int64_t obs = Metar::resolveObsTime(r.day, r.hour, r.minute, nowUtc());
    portENTER_CRITICAL(&s_lock);
    s_st.report = r;
    strncpy(s_st.raw, raw, sizeof s_st.raw - 1);
    s_st.raw[sizeof s_st.raw - 1] = 0;
    s_st.obsUtc = obs;
    s_st.fetchedUtc = fetchedUtc;
    s_st.fromCache = fromCache;
    s_st.hasReport = true;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

void clearReport()
{
    portENTER_CRITICAL(&s_lock);
    s_st.hasReport = false;
    s_st.raw[0] = 0;
    s_st.obsUtc = 0;
    s_st.fetchedUtc = 0;
    s_st.fromCache = false;
    memset(&s_st.report, 0, sizeof s_st.report);
    portEXIT_CRITICAL(&s_lock);
}

void saveCache(const char* station, const char* raw, int64_t fetchedUtc, int64_t obsUtc)
{
    CacheRec c{};
    c.version = kCacheVersion;
    memcpy(c.station, station, 5);
    c.fetchedUtc = fetchedUtc;
    c.obsUtc = obsUtc;
    c.rawLen = uint16_t(strnlen(raw, Metar::kMaxRaw));
    memcpy(c.raw, raw, c.rawLen);
    const size_t len = offsetof(CacheRec, raw) + c.rawLen;
    if (!SettingsStore::setBlob(kCacheKey, &c, len)) {
        LOGW("wx", "cache write failed");
    }
}

void loadCache()
{
    CacheRec c{};
    const size_t n = SettingsStore::getBlob(kCacheKey, &c, sizeof c);
    if (n == 0) {
        return;
    }
    char stn[5];
    currentStation(stn);
    if (n < offsetof(CacheRec, raw) || c.version != kCacheVersion || c.rawLen > Metar::kMaxRaw ||
        n != offsetof(CacheRec, raw) + c.rawLen || memcmp(c.station, stn, 5) != 0) {
        LOGW("wx", "cache invalid; erased");
        SettingsStore::eraseBlob(kCacheKey);
        return;
    }
    char raw[Metar::kMaxRaw + 1];
    memcpy(raw, c.raw, c.rawLen);
    raw[c.rawLen] = 0;
    if (!loadRaw(raw, c.fetchedUtc, true)) {
        SettingsStore::eraseBlob(kCacheKey);
        return;
    }
    portENTER_CRITICAL(&s_lock);
    if (s_st.obsUtc == 0) {
        s_st.obsUtc = c.obsUtc;   // clock not valid yet at boot: keep the stored observation time
    }
    portEXIT_CRITICAL(&s_lock);
    LOGI("wx", "cache: %s obs=%02d%02dZ", stn, s_st.report.hour, s_st.report.minute);
}

void setResult(WxResult r)
{
    portENTER_CRITICAL(&s_lock);
    s_st.lastResult = r;
    s_st.lastAttemptUtc = nowUtc();
    portEXIT_CRITICAL(&s_lock);
}

void setBusy(WxBusy b)
{
    portENTER_CRITICAL(&s_lock);
    s_st.busy = b;
    portEXIT_CRITICAL(&s_lock);
    publish();
}

void onStationChanged()
{
    char stn[5];
    currentStation(stn);
    clearReport();
    SettingsStore::eraseBlob(kCacheKey);
    portENTER_CRITICAL(&s_lock);
    memcpy(s_st.station, stn, 5);
    s_st.lastResult = WxResult::None;
    portEXIT_CRITICAL(&s_lock);
    publish();
    LOGI("wx", "station %s", stn);
    if (!NetService::requestWeather()) {
        markNoWifi();
    }
}

// Reads the HTTPS response into s_rx. Returns the WxResult for transport-level failures, or Ok with `res` set.
WxResult exchange(const char* station, uint32_t deadlineMs, core::HttpResp::Result& res, size_t& got, int& tlsErr)
{
    WiFiClientSecure c;
#ifdef WX_CERT_SELFTEST
    // T017 self-test build only: trust just the fallback root (G5), which does not sign this server's chain, so the
    // handshake must fail with `NetFail` (verification error). Never enabled in normal builds.
    c.setCACert(strstr(kWxRootCerts, "-----END CERTIFICATE-----") + 26);
#else
    c.setCACert(kWxRootCerts);
#endif
    const int32_t left = int32_t(deadlineMs - millis());
    if (left <= 500) {
        return WxResult::NetFail;
    }
    c.setHandshakeTimeout(uint32_t(left / 1000 + 1));
    if (!c.connect(kHost, kPort, left)) {
        char e[64];
        tlsErr = c.lastError(e, sizeof e);
        LOGW("wx", "connect/tls failed err=%d (%s)", tlsErr, e);
        c.stop();
        return WxResult::NetFail;
    }
    char req[200];
    const int n = snprintf(req, sizeof req,
                           "GET /api/data/metar?ids=%s&format=raw HTTP/1.1\r\nHost: %s\r\nUser-Agent: AeroDeck/%s\r\n"
                           "Accept: text/plain\r\nConnection: close\r\n\r\n",
                           station, kHost, AD_FW_VERSION);
    if (c.write(reinterpret_cast<const uint8_t*>(req), size_t(n)) != size_t(n)) {
        c.stop();
        return WxResult::NetFail;
    }
    got = 0;
    res = core::HttpResp::Result{};
    for (;;) {
        if (int32_t(deadlineMs - millis()) <= 0) {
            c.stop();
            LOGW("wx", "timeout after %u bytes", unsigned(got));
            return WxResult::NetFail;
        }
        int avail = c.available();
        if (avail > 0) {
            if (got >= kRxCap) {
                c.stop();
                return WxResult::ServerError;   // larger than any valid response
            }
            int r = c.read(reinterpret_cast<uint8_t*>(s_rx + got), kRxCap - got);
            if (r > 0) {
                got += size_t(r);
            }
            res = core::HttpResp::parse(s_rx, got, false);
        } else {
            const bool closed = !c.connected();
            res = core::HttpResp::parse(s_rx, got, closed);
            if (closed && res.state == core::HttpResp::State::Incomplete) {
                res.state = core::HttpResp::State::Error;
            }
            if (res.state == core::HttpResp::State::Incomplete) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }
        if (res.state != core::HttpResp::State::Incomplete) {
            break;
        }
    }
    c.stop();
    return res.state == core::HttpResp::State::Complete ? WxResult::Ok : WxResult::ServerError;
}

// ---- serial commands (contracts/serial_commands.md) ----

void printFields(const Metar::Report& r, core::Reply& out)
{
    char wind[40], clouds[40], line[220];
    Metar::formatWind(r, wind, sizeof wind);
    Metar::formatClouds(r, clouds, sizeof clouds);
    char ws[16], gs[8], vs[16];
    if (r.windSpdKt < 0) {
        snprintf(ws, sizeof ws, "--");
    } else if (r.windDir == Metar::kWindVrb) {
        snprintf(ws, sizeof ws, "VRB/%02d", r.windSpdKt);
    } else {
        snprintf(ws, sizeof ws, "%03d/%02d", r.windDir, r.windSpdKt);
    }
    if (r.windGustKt >= 0) snprintf(gs, sizeof gs, "%d", r.windGustKt); else snprintf(gs, sizeof gs, "--");
    if (r.windVarFrom >= 0) snprintf(vs, sizeof vs, "%03d-%03d", r.windVarFrom, r.windVarTo); else snprintf(vs, sizeof vs, "--");
    const int32_t ceil = Metar::ceilingFt(r);
    char cs[12];
    if (ceil == Metar::kNoCeiling) snprintf(cs, sizeof cs, "none"); else if (ceil < 0) snprintf(cs, sizeof cs, "--"); else snprintf(cs, sizeof cs, "%ld", (long)ceil);
    char ts[8], ds[8], qs[8];
    if (r.tempC == Metar::kNoTemp) snprintf(ts, sizeof ts, "--"); else snprintf(ts, sizeof ts, "%d", r.tempC);
    if (r.dewC == Metar::kNoTemp) snprintf(ds, sizeof ds, "--"); else snprintf(ds, sizeof ds, "%d", r.dewC);
    if (r.qnhHpa < 0) snprintf(qs, sizeof qs, "--"); else snprintf(qs, sizeof qs, "%d", r.qnhHpa);
    snprintf(line, sizeof line,
             "stn=%s obs=%02d%02d%02dZ wind=%s gust=%s var=%s vis_m=%ld vis=\"%s\" cavok=%d ceil_ft=%s clouds=\"%s\" "
             "t=%s dp=%s qnh=%s cat=%s",
             r.station, r.day, r.hour, r.minute, ws, gs, vs, (long)r.visM, r.visText[0] ? r.visText : "--",
             r.cavok ? 1 : 0, cs, clouds, ts, ds, qs, Metar::categoryName(Metar::category(r)));
    out.line(line);
    (void)wind;
}

void cmdStatus(int, const char* const*, core::Reply& r)
{
    static WeatherSnapshot s;
    s = s_snap.read();
    const int64_t now = nowUtc();
    char b[240];
    snprintf(b, sizeof b,
             "stn=%s has=%d cache=%d busy=%s last=%s obs_utc=%lld age_s=%lld stale=%d fetched_utc=%lld cat=%s",
             s.station, s.hasReport ? 1 : 0, s.fromCache ? 1 : 0, busyName(s.busy), resultName(s.lastResult),
             (long long)s.obsUtc, (long long)(s.obsUtc && now ? now - s.obsUtc : -1),
             isStale(s, now, now != 0) ? 1 : 0, (long long)s.fetchedUtc,
             s.hasReport ? Metar::categoryName(Metar::category(s.report)) : "--");
    r.ok();
    r.line(b);
    char raw[Metar::kMaxRaw + 8];
    snprintf(raw, sizeof raw, "raw=%s", s.hasReport ? s.raw : "");
    r.line(raw);
}

void cmdFields(int, const char* const*, core::Reply& r)
{
    static WeatherSnapshot s;
    s = s_snap.read();
    if (!s.hasReport) {
        r.err("no data");
        return;
    }
    r.ok();
    printFields(s.report, r);
}

void cmdFetch(int, const char* const*, core::Reply& r)
{
    const WxBusy before = s_snap.read().busy;
    if (before != WxBusy::Idle) {
        r.line("OK already pending");
        return;
    }
    if (!NetService::requestWeather()) {
        markNoWifi();
        r.err("wifi not set up");
        return;
    }
    r.line("OK fetching");
}

void cmdSet(int, const char* const* argv, core::Reply& r)
{
    if (!core::validateString(SettingKey::WxStation, argv[0])) {
        r.err("station must be 4 letters A-Z");
        return;
    }
    SettingsStore::setStr(SettingKey::WxStation, argv[0]);
    char stn[8];
    SettingsStore::getStr(SettingKey::WxStation, stn, sizeof stn);
    char b[24];
    snprintf(b, sizeof b, "OK stn=%s", stn);
    r.line(b);
}

void cmdDecode(int argc, const char* const* argv, core::Reply& r)
{
    static Metar::Report rep;
    if (!Metar::decode(argv[0], rep)) {
        r.err("no station/time");
        return;
    }
    r.ok();
    printFields(rep, r);
    if (argc >= 2 && strcmp(argv[1], "show") == 0) {
        // RAM only: render on the WX page as if just observed (not cached, not persisted).
        portENTER_CRITICAL(&s_lock);
        s_st.report = rep;
        strncpy(s_st.raw, argv[0], sizeof s_st.raw - 1);
        s_st.raw[sizeof s_st.raw - 1] = 0;
        s_st.obsUtc = nowUtc();
        s_st.fetchedUtc = nowUtc();
        s_st.fromCache = false;
        s_st.hasReport = true;
        portEXIT_CRITICAL(&s_lock);
        publish();
    }
}

void registerCommands()
{
    SerialConsole::add("wx", 0, cmdStatus);
    SerialConsole::add("wx fields", 0, cmdFields);
    SerialConsole::add("wx fetch", 0, cmdFetch);
    SerialConsole::add("wx set", 1, cmdSet);
    SerialConsole::add("wx decode", 1, cmdDecode);
    SerialConsole::add("wx clear", 0, [](int, const char* const*, core::Reply& r) {
        clearReport();
        SettingsStore::eraseBlob(kCacheKey);
        publish();
        r.ok();
    });
    SerialConsole::add("wx age", 1, [](int, const char* const* argv, core::Reply& r) {
        s_ageOverrideS = strtoll(argv[0], nullptr, 10);
        publish();
        r.ok();
    });
}

} // namespace

const char* resultName(WxResult r)
{
    switch (r) {
    case WxResult::None: return "None";
    case WxResult::Ok: return "Ok";
    case WxResult::NoWifi: return "NoWifi";
    case WxResult::NetFail: return "NetFail";
    case WxResult::NoReport: return "NoReport";
    case WxResult::ServerError: return "ServerError";
    default: return "?";
    }
}

const char* busyName(WxBusy b)
{
    switch (b) {
    case WxBusy::Idle: return "Idle";
    case WxBusy::Pending: return "Pending";
    case WxBusy::Fetching: return "Fetching";
    default: return "?";
    }
}

bool isStale(const WeatherSnapshot& s, int64_t now, bool clockValid)
{
    if (!clockValid) {
        return true;
    }
    if (s.ageOverrideS >= 0) {
        return s.ageOverrideS > Metar::kStaleAfterS;
    }
    return Metar::isStale(s.obsUtc, now);
}

const Snapshot<WeatherSnapshot>& status() { return s_snap; }

void markPending()
{
    portENTER_CRITICAL(&s_lock);
    if (s_st.busy == WxBusy::Idle) {
        s_st.busy = WxBusy::Pending;
    }
    portEXIT_CRITICAL(&s_lock);
    publish();
}

void markNoWifi()
{
    setResult(WxResult::NoWifi);
    publish();
}

void fetchOnNet(uint32_t deadlineMs)
{
    char stn[5];
    currentStation(stn);
    setBusy(WxBusy::Fetching);
    const uint32_t t0 = millis();
    core::HttpResp::Result res{};
    size_t got = 0;
    int tlsErr = 0;
    WxResult result = s_rx ? exchange(stn, deadlineMs, res, got, tlsErr) : WxResult::ServerError;
    static char line[Metar::kMaxRaw + 1];
    line[0] = 0;
    if (result == WxResult::Ok) {
        if (res.chunked || (res.status != 200 && res.status != 204)) {
            result = WxResult::ServerError;
            LOGW("wx", "fetch: ServerError (http=%d chunked=%d)", res.status, res.chunked ? 1 : 0);
        } else if (res.status == 204 || res.bodyLen == 0) {
            result = WxResult::NoReport;
        } else if (!Metar::pickNewest(s_rx + res.bodyOff, res.bodyLen, stn, line, sizeof line)) {
            result = WxResult::ServerError;
            LOGW("wx", "fetch: ServerError (no line for %s)", stn);
        }
    }
    // Station changed while fetching: discard and fetch the new one (data-model §3).
    char now[5];
    currentStation(now);
    if (memcmp(now, stn, 5) != 0) {
        LOGI("wx", "fetch for %s discarded (station now %s)", stn, now);
        setResult(WxResult::None);
        setBusy(WxBusy::Idle);
        NetService::requestWeather();
        return;
    }
    if (result == WxResult::Ok) {
        static Metar::Report probe;
        if (!Metar::decode(line, probe)) {
            result = WxResult::ServerError;
            LOGW("wx", "fetch: ServerError (undecodable)");
        } else if (probe.nil) {
            result = WxResult::NoReport;
        } else {
            const int64_t prevObs = s_snap.read().obsUtc;
            loadRaw(line, nowUtc(), false);
            int64_t obs;
            portENTER_CRITICAL(&s_lock);
            obs = s_st.obsUtc;
            portEXIT_CRITICAL(&s_lock);
            if (obs != prevObs || prevObs == 0) {
                saveCache(stn, line, nowUtc(), obs);
                LOGI("wx", "raw %s", line);
            }
        }
    }
    setResult(result);
    portENTER_CRITICAL(&s_lock);
    s_st.busy = WxBusy::Idle;
    const Metar::Category cat = s_st.hasReport ? Metar::category(s_st.report) : Metar::Category::Unknown;
    const int hh = s_st.report.hour, mm = s_st.report.minute;
    portEXIT_CRITICAL(&s_lock);
    publish();
    EventBus::publish(EventType::WeatherUpdated, uint32_t(result));
    if (result == WxResult::Ok) {
        LOGI("wx", "fetch: Ok stn=%s obs=%02d%02dZ cat=%s ms=%lu", stn, hh, mm, Metar::categoryName(cat),
             (unsigned long)(millis() - t0));
    } else {
        LOGW("wx", "fetch: %s stn=%s ms=%lu tls=%d", resultName(result), stn, (unsigned long)(millis() - t0), tlsErr);
    }
}

void failNoNetwork(const char* why)
{
    setResult(WxResult::NetFail);
    portENTER_CRITICAL(&s_lock);
    s_st.busy = WxBusy::Idle;
    portEXIT_CRITICAL(&s_lock);
    publish();
    EventBus::publish(EventType::WeatherUpdated, uint32_t(WxResult::NetFail));
    LOGW("wx", "fetch: NetFail (wifi %s)", why);
}

void tick(uint32_t)
{
    Event e;
    while (s_events && xQueueReceive(s_events, &e, 0) == pdTRUE) {
        if (e.type == EventType::SettingsChanged && SettingKey(e.a) == SettingKey::WxStation) {
            onStationChanged();   // every event, even if unchanged (restore defaults, analyze U3)
        } else if (e.type == EventType::ClockChanged) {
            // Re-resolve the observation time with the new clock (cached report loaded before the first sync).
            bool redo = false;
            Metar::Report rep;
            portENTER_CRITICAL(&s_lock);
            if (s_st.hasReport) {
                rep = s_st.report;
                redo = true;
            }
            portEXIT_CRITICAL(&s_lock);
            if (redo) {
                const int64_t obs = Metar::resolveObsTime(rep.day, rep.hour, rep.minute, nowUtc());
                if (obs) {
                    portENTER_CRITICAL(&s_lock);
                    s_st.obsUtc = obs;
                    portEXIT_CRITICAL(&s_lock);
                    publish();
                }
            }
        }
    }
}

void begin()
{
    mbedtls_platform_set_calloc_free(wxCalloc, wxFree);
    s_rx = static_cast<char*>(heap_caps_malloc(kRxCap + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!s_rx) {
        s_rx = static_cast<char*>(malloc(kRxCap + 1));
    }
    s_st = WeatherSnapshot{};
    s_st.ageOverrideS = -1;
    currentStation(s_st.station);
    loadCache();
    s_events = EventBus::subscribe(8);
    publish();
    registerCommands();
    LOGI("wx", "ready; stn=%s cache=%d", s_st.station, s_st.hasReport ? 1 : 0);
}

} // namespace WeatherService
