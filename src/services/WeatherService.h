// AeroDeck weather service (S3): METAR fetch over HTTPS, decode, cache, status snapshot.
// The fetch itself (`fetchOnNet`) runs on the S2 `net` task inside a Wi-Fi lease; everything else on the svc task.
// UI reads Snapshot<WeatherSnapshot> and asks for a refresh through NetService::requestWeather().
// ADR-012: TLS/socket code lives only in src/services/. Contracts: specs/004-metar-weather/contracts/.
#pragma once
#include <cstdint>
#include "core/Metar.h"
#include "services/Snapshot.h"

enum class WxBusy : uint8_t { Idle, Pending, Fetching };
enum class WxResult : uint8_t { None, Ok, NoWifi, NetFail, NoReport, ServerError };

struct WeatherSnapshot {
    char station[5];
    bool hasReport;            // a decoded report for `station` is present (fresh or cached)
    core::Metar::Report report;
    char raw[core::Metar::kMaxRaw + 1];
    int64_t obsUtc;            // 0 = unknown
    int64_t fetchedUtc;        // 0 = never
    bool fromCache;
    WxBusy busy;
    WxResult lastResult;
    int64_t lastAttemptUtc;
    int64_t ageOverrideS;      // `ad wx age` test hook (-1 = off)
    uint32_t seq;
};

namespace WeatherService {
void begin();                              // before NetService::begin(): TLS allocator, cache, commands
void tick(uint32_t nowMs);                 // svc task: station changes, clock changes
const Snapshot<WeatherSnapshot>& status();
// net task only: fetch the current station's METAR before deadlineMs (millis()). Wi-Fi must be connected.
void fetchOnNet(uint32_t deadlineMs);
void markPending();                        // NetService queued a weather fetch
void markNoWifi();                         // a fetch was requested but Wi-Fi is not set up
void failNoNetwork(const char* why);       // net task: the Wi-Fi connection for a weather fetch failed
const char* resultName(WxResult r);
const char* busyName(WxBusy b);
// Observation age used for STALE (honours the `ad wx age` hook). Returns true when stale.
bool isStale(const WeatherSnapshot& s, int64_t nowUtc, bool clockValid);
} // namespace WeatherService
