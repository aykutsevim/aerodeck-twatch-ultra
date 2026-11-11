#include "services/NetService.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <WiFiUdp.h>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include "core/ClockMath.h"
#include "core/Ntp.h"
#include "core/SyncSchedule.h"
#include "services/AlertManager.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/SetupPortal.h"
#include "services/TimeService.h"
#include "services/WeatherService.h"

using core::SettingKey;

namespace NetService {

namespace {

enum class Cmd : uint8_t { Sync, Test, Scan, SetupStart, Weather };

constexpr uint32_t kTxnTimeoutMs = 15000;   // FR-008: whole transaction
constexpr uint32_t kNtpWaitMs = 3000;       // per server
constexpr uint32_t kWxStepMs = 15000;       // S3 FR-003: weather step inside a sync session
const char* const kServers[] = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};

QueueHandle_t s_cmds;
Snapshot<NetStatus> s_snap;
NetStatus s_st;                 // owned by the net task (plus atomic fields from svc)
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
core::SyncSchedule s_sched;
volatile uint8_t s_lastReason;  // last STA disconnect reason (event callback)
volatile bool s_setupStop;
volatile int64_t s_ageOverrideS = -1;   // `ad wifi age` test hook
uint32_t s_seq;
bool s_lastConfigured;
int32_t s_lastSyncMin = -1;
bool s_unsyncRaised;
std::atomic<bool> s_wxPending{false};   // S3: a weather fetch is requested (coalescing, research R4)

void publish()
{
    portENTER_CRITICAL(&s_lock);
    s_st.seq = ++s_seq;
    s_st.nextDueMs = s_sched.nextMs();
    s_st.wifiOn = PowerService::refcount(core::Rail::Wifi) > 0;
    NetStatus copy = s_st;
    portEXIT_CRITICAL(&s_lock);
    s_snap.write(copy);
    EventBus::publish(EventType::NetStatusChanged);
}

void setBusy(NetBusy b)
{
    s_st.busy = b;
    publish();
}

// PowerService hook for Rail::Wifi (constitution III). Called on the task that releases/acquires the lease.
void wifiRail(bool on)
{
    if (!on) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
    // ON: the driver starts on the next WiFi.mode()/begin() call made by the lease holder.
}

void onDisconnected(arduino_event_id_t, arduino_event_info_t info)
{
    s_lastReason = info.wifi_sta_disconnected.reason;
}

bool credentials(char* ssid, size_t ns, char* psk, size_t np)
{
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, ns);
    SettingsStore::getStr(SettingKey::WifiPsk, psk, np);
    return ssid[0] != '\0';
}

NetResult connectSta(const char* ssid, const char* psk, uint32_t deadlineMs)
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    s_lastReason = 0;
    WiFi.begin(ssid, psk);
    while (int32_t(deadlineMs - millis()) > 0) {
        if (WiFi.status() == WL_CONNECTED) {
            return NetResult::Ok;
        }
        switch (s_lastReason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return NetResult::WrongPassword;
        case WIFI_REASON_NO_AP_FOUND:
            return NetResult::NotFound;
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return NetResult::Timeout;
}

// One SNTP query per server until one answers. Returns Ok/NoTimeResponse/Rejected.
NetResult queryNtp(uint32_t deadlineMs, core::Ntp::Result& out, const char** server)
{
    WiFiUDP udp;
    if (!udp.begin(2390)) {
        return NetResult::NoTimeResponse;
    }
    bool rejected = false;
    for (const char* host : kServers) {
        if (int32_t(deadlineMs - millis()) <= 0) {
            break;
        }
        uint8_t req[core::Ntp::kPacketSize], rsp[core::Ntp::kPacketSize];
        timeval tv;
        gettimeofday(&tv, nullptr);
        core::Ntp::buildRequest(req, int64_t(tv.tv_sec), uint16_t(tv.tv_usec / 1000));
        if (!udp.beginPacket(host, 123)) {
            continue;   // DNS failure: try the next server
        }
        udp.write(req, sizeof req);
        udp.endPacket();
        const uint32_t until = millis() + kNtpWaitMs;
        while (int32_t(until - millis()) > 0 && int32_t(deadlineMs - millis()) > 0) {
            int n = udp.parsePacket();
            if (n >= int(sizeof rsp)) {
                udp.read(rsp, sizeof rsp);
                gettimeofday(&tv, nullptr);
                if (core::Ntp::parseResponse(rsp, sizeof rsp, req, int64_t(tv.tv_sec), uint16_t(tv.tv_usec / 1000), out)) {
                    int y, mo, d, h, mi, s;
                    core::civilFromUtc(out.unixSec, y, mo, d, h, mi, s);
                    if (!core::clockPlausible(y)) {   // FR-009
                        rejected = true;
                        break;
                    }
                    udp.stop();
                    *server = host;
                    return NetResult::Ok;
                }
            } else if (n > 0) {
                udp.flush();
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    udp.stop();
    return rejected ? NetResult::Rejected : NetResult::NoTimeResponse;
}

// Connect + NTP. applyClock=false for the test connection (FR-003).
NetResult syncOrTest(bool applyClock)
{
    char ssid[33], psk[64];
    if (!credentials(ssid, sizeof ssid, psk, sizeof psk)) {
        return NetResult::NotFound;
    }
    const uint32_t deadline = millis() + kTxnTimeoutMs;
    RailLease wifi(core::Rail::Wifi);   // released (Wi-Fi OFF) when this scope ends
    NetResult r = connectSta(ssid, psk, deadline);
    memset(psk, 0, sizeof psk);
    if (r != NetResult::Ok) {
        return r;
    }
    core::Ntp::Result t{};
    const char* server = "";
    r = queryNtp(deadline, t, &server);
    if (r != NetResult::Ok) {
        if (applyClock) {
            s_wxPending = false;
            WeatherService::fetchOnNet(millis() + kWxStepMs);   // FR-004: independent of the time result
        }
        return r;
    }
    if (applyClock) {
        // Set on the next whole second so the RTC (1 s resolution) lands within a few ms.
        const uint32_t wait = 1000u - t.ms;
        vTaskDelay(pdMS_TO_TICKS(wait));
        const int64_t utc = t.unixSec + 1;
        TimeService::set(utc);
        SettingsStore::set(SettingKey::LastSync, int32_t(utc), true);
        s_st.lastSyncUtc = utc;
        LOGI("net", "sync: Ok offset_ms=%lld server=%s", (long long)t.offsetMs, server);
        // S3: weather in the same radio session (own 15 s budget; clears any pending request).
        s_wxPending = false;
        WeatherService::fetchOnNet(millis() + kWxStepMs);
    } else {
        LOGI("net", "test: Ok offset_ms=%lld server=%s", (long long)t.offsetMs, server);
    }
    return NetResult::Ok;
}

// S3: on-demand weather transaction: connect + fetch share one 15 s deadline (spec FR-003).
void doWeather()
{
    if (!s_wxPending.exchange(false)) {
        return;   // a sync already fetched since the request (research R4)
    }
    char ssid[33], psk[64];
    if (!credentials(ssid, sizeof ssid, psk, sizeof psk)) {
        WeatherService::markNoWifi();
        return;
    }
    const uint32_t deadline = millis() + kTxnTimeoutMs;
    RailLease wifi(core::Rail::Wifi);
    NetResult r = connectSta(ssid, psk, deadline);
    memset(psk, 0, sizeof psk);
    if (r != NetResult::Ok) {
        WeatherService::failNoNetwork(resultName(r));
        return;
    }
    WeatherService::fetchOnNet(deadline);
}

void doScan()
{
    RailLease wifi(core::Rail::Wifi);
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, false);
    LOGI("net", "scan: %d networks", n < 0 ? 0 : n);
    for (int i = 0; i < n && i < 20; i++) {   // the driver already returns strongest first
        LOGI("net", "scan: %d %s", WiFi.RSSI(i), WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();
}

void setupProgress(const char* ssid, const char* code, uint16_t remainingS)
{
    portENTER_CRITICAL(&s_lock);
    strncpy(s_st.setupSsid, ssid ? ssid : "", sizeof s_st.setupSsid - 1);
    strncpy(s_st.setupCode, code ? code : "", sizeof s_st.setupCode - 1);
    s_st.setupRemainingS = remainingS;
    portEXIT_CRITICAL(&s_lock);
    publish();
}

void doSetup()
{
    char ssid[33] = {0}, psk[64] = {0};
    s_setupStop = false;
    NetResult r;
    {
        RailLease wifi(core::Rail::Wifi);   // hotspot powered only for this session (FR-004a)
        r = SetupPortal::run(s_setupStop, setupProgress, ssid, sizeof ssid, psk, sizeof psk);
    }
    setupProgress("", "", 0);   // clears the code from the status snapshot
    if (r != NetResult::Ok) {
        s_st.lastResult = NetResult::Cancelled;
        LOGI("net", "setup: %s", r == NetResult::Timeout ? "expired" : "cancelled");
        return;
    }
    SettingsStore::setStr(SettingKey::WifiSsid, ssid);
    SettingsStore::setStr(SettingKey::WifiPsk, psk);
    memset(psk, 0, sizeof psk);
    LOGI("net", "setup: credentials saved for '%s'", ssid);
    vTaskDelay(pdMS_TO_TICKS(200));   // let the settings cache settle
    setBusy(NetBusy::Testing);
    s_st.lastResult = syncOrTest(true);   // test by syncing straight away
    LOGI("net", "test: %s", resultName(s_st.lastResult));
}

void netTask(void*)
{
    for (;;) {
        Cmd c;
        if (xQueueReceive(s_cmds, &c, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (c) {
        case Cmd::Sync: {
            setBusy(NetBusy::Syncing);
            NetResult r = syncOrTest(true);
            s_st.lastResult = r;
            if (r == NetResult::Ok) {
                s_sched.onSuccess(millis());
            } else {
                s_sched.onFailure(millis());
                LOGW("net", "sync: %s", resultName(r));
            }
            break;
        }
        case Cmd::Test: {
            setBusy(NetBusy::Testing);
            NetResult r = syncOrTest(false);
            s_st.lastResult = r;
            if (r != NetResult::Ok) {
                LOGW("net", "test: %s", resultName(r));
            }
            break;
        }
        case Cmd::Scan:
            setBusy(NetBusy::Scanning);
            doScan();
            break;
        case Cmd::SetupStart:
            setBusy(NetBusy::Setup);
            doSetup();
            break;
        case Cmd::Weather:
            setBusy(NetBusy::Weather);
            doWeather();
            break;
        }
        setBusy(NetBusy::Idle);
    }
}

void post(Cmd c)
{
    if (s_cmds) {
        xQueueSend(s_cmds, &c, 0);
    }
}

// ---- serial commands (contracts/serial_commands.md) ----

void cmdStatus(int, const char* const*, core::Reply& r)
{
    NetStatus st = s_snap.read();
    char ssid[33];
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, sizeof ssid);
    const int64_t now = int64_t(time(nullptr));
    char b[160];
    snprintf(b, sizeof b, "configured=%d ssid=%s psk=%s busy=%s last=%s last_sync_utc=%lld age_s=%lld next_s=%ld wifi=%s",
             st.configured ? 1 : 0, ssid[0] ? ssid : "-", st.configured ? "***" : "-", busyName(st.busy),
             resultName(st.lastResult), (long long)st.lastSyncUtc,
             (long long)(st.lastSyncUtc ? now - st.lastSyncUtc : -1),
             (long)(int32_t(st.nextDueMs - millis()) / 1000), st.wifiOn ? "ON" : "OFF");
    r.ok();
    r.line(b);
}

void cmdSet(int, const char* const* argv, core::Reply& r)
{
    if (!core::validateString(SettingKey::WifiSsid, argv[0])) {
        r.err("ssid 1..32 chars");
        return;
    }
    if (!core::validateString(SettingKey::WifiPsk, argv[1])) {
        r.err("psk 8..63 chars");
        return;
    }
    SettingsStore::setStr(SettingKey::WifiSsid, argv[0]);
    SettingsStore::setStr(SettingKey::WifiPsk, argv[1]);
    LOGI("net", "credentials saved for '%s'", argv[0]);   // SSID only, never the PSK
    post(Cmd::Test);
    r.line("OK testing");
}

} // namespace

const char* resultName(NetResult r)
{
    switch (r) {
    case NetResult::None: return "None";
    case NetResult::Ok: return "Ok";
    case NetResult::WrongPassword: return "WrongPassword";
    case NetResult::NotFound: return "NotFound";
    case NetResult::NoTimeResponse: return "NoTimeResponse";
    case NetResult::Timeout: return "Timeout";
    case NetResult::Rejected: return "Rejected";
    case NetResult::Cancelled: return "Cancelled";
    default: return "?";
    }
}

const char* busyName(NetBusy b)
{
    switch (b) {
    case NetBusy::Idle: return "Idle";
    case NetBusy::Syncing: return "Syncing";
    case NetBusy::Testing: return "Testing";
    case NetBusy::Scanning: return "Scanning";
    case NetBusy::Setup: return "Setup";
    case NetBusy::Weather: return "Weather";
    default: return "?";
    }
}

const Snapshot<NetStatus>& status() { return s_snap; }
void requestSync() { post(Cmd::Sync); }

bool requestWeather()
{
    char ssid[33];
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, sizeof ssid);
    if (ssid[0] == '\0') {
        return false;
    }
    if (!s_wxPending.exchange(true)) {
        WeatherService::markPending();
        post(Cmd::Weather);
    }
    return true;
}
void requestTest() { post(Cmd::Test); }
void startSetup() { post(Cmd::SetupStart); }
void stopSetup() { s_setupStop = true; }

void forget()
{
    SettingsStore::clearStr(SettingKey::WifiSsid);
    SettingsStore::clearStr(SettingKey::WifiPsk);
    s_sched.onForgotten();
    LOGI("net", "network forgotten");
}

void tick(uint32_t now)
{
    char ssid[33];
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, sizeof ssid);
    const bool configured = ssid[0] != '\0';
    if (configured != s_lastConfigured) {
        s_lastConfigured = configured;
        s_st.configured = configured;
        if (configured) {
            s_sched.onConfigured(now);
        } else {
            s_sched.onForgotten();
        }
        publish();
    }
    const int32_t syncMin = SettingsStore::get(SettingKey::SyncMin);
    if (syncMin != s_lastSyncMin) {
        s_lastSyncMin = syncMin;
        s_sched.setInterval(uint16_t(syncMin));
    }
    if (configured && s_st.busy == NetBusy::Idle && s_sched.due(now) && uxQueueMessagesWaiting(s_cmds) == 0) {
        post(Cmd::Sync);
    }

    // TIME UNSYNC (FR-018)
    const int64_t nowUtc = int64_t(time(nullptr));
    int64_t last = int64_t(SettingsStore::get(SettingKey::LastSync));
    if (s_ageOverrideS >= 0) {
        last = nowUtc - s_ageOverrideS;
    }
    const bool unsynced = core::SyncSchedule::isUnsynced(nowUtc, last, configured);
    if (unsynced != s_unsyncRaised) {
        s_unsyncRaised = unsynced;
        if (unsynced) {
            AlertManager::raise(core::AlertId::TimeUnsync);   // ADVISORY
        } else {
            AlertManager::clear(core::AlertId::TimeUnsync);
        }
    }
}

void begin()
{
    PowerService::setRailHook(core::Rail::Wifi, wifiRail);
    WiFi.onEvent(onDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    char ssid[33];
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, sizeof ssid);
    s_lastConfigured = ssid[0] != '\0';
    s_st = NetStatus{};
    s_st.configured = s_lastConfigured;
    s_st.lastSyncUtc = int64_t(SettingsStore::get(SettingKey::LastSync));
    s_lastSyncMin = SettingsStore::get(SettingKey::SyncMin);
    s_sched.onBoot(millis(), s_lastConfigured, uint16_t(s_lastSyncMin));
    s_cmds = xQueueCreate(8, sizeof(Cmd));
    publish();
    xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 1, nullptr, 0);

    SerialConsole::add("wifi", 0, cmdStatus);
    SerialConsole::add("wifi set", 2, cmdSet);
    SerialConsole::add("wifi forget", 0, [](int, const char* const*, core::Reply& r) {
        forget();
        r.ok();
    });
    SerialConsole::add("wifi test", 0, [](int, const char* const*, core::Reply& r) {
        post(Cmd::Test);
        r.line("OK testing");
    });
    SerialConsole::add("wifi scan", 0, [](int, const char* const*, core::Reply& r) {
        post(Cmd::Scan);
        r.line("OK scanning");
    });
    SerialConsole::add("wifi setup", 0, [](int, const char* const*, core::Reply& r) {
        post(Cmd::SetupStart);
        r.line("OK setup starting (code shown on the watch)");
    });
    SerialConsole::add("wifi setup stop", 0, [](int, const char* const*, core::Reply& r) {
        stopSetup();
        r.ok();
    });
    SerialConsole::add("wifi age", 1, [](int, const char* const* argv, core::Reply& r) {
        s_ageOverrideS = strtoll(argv[0], nullptr, 10);
        r.ok();
    });
    SerialConsole::add("sync", 0, [](int, const char* const*, core::Reply& r) {
        post(Cmd::Sync);
        r.line("OK syncing");
    });
    LOGI("net", "ready; configured=%d", s_lastConfigured ? 1 : 0);
}

} // namespace NetService
