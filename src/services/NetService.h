// AeroDeck network service (S2): Wi-Fi credentials, test connection, SNTP time sync, phone setup hotspot.
// All blocking work runs on the `net` task (core 0); callers post commands and read the status snapshot.
// ADR-012: the SoC Wi-Fi stack is a platform service, used only in src/services/. Wi-Fi power goes through
// PowerService (Rail::Wifi, constitution III).
#pragma once
#include <cstdint>
#include "services/Snapshot.h"

enum class NetBusy : uint8_t { Idle, Syncing, Testing, Scanning, Setup, Weather };
enum class NetResult : uint8_t { None, Ok, WrongPassword, NotFound, NoTimeResponse, Timeout, Rejected, Cancelled };

struct NetStatus {
    bool configured;
    NetBusy busy;
    NetResult lastResult;
    int64_t lastSyncUtc;      // 0 = never
    uint32_t nextDueMs;
    char setupSsid[16];       // "AeroDeck-1A2B" while setup is active
    char setupCode[9];        // one-time access code: shown on the watch only, never logged
    uint16_t setupRemainingS;
    bool wifiOn;
    uint32_t seq;
};

namespace NetService {
void begin();                            // rail hook, serial commands, net task
const Snapshot<NetStatus>& status();
void requestSync();
// S3: queue a weather fetch (connect + fetch, one 15 s deadline). Coalesced: repeated calls before it runs, or a
// sync that fetches meanwhile, produce exactly one fetch. Returns false when Wi-Fi is not set up.
bool requestWeather();
void requestTest();
void startSetup();
void stopSetup();
void forget();
void tick(uint32_t nowMs);               // svc task, 1 Hz: schedule, TIME UNSYNC
const char* resultName(NetResult r);
const char* busyName(NetBusy b);
} // namespace NetService
