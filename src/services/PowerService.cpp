#include "services/PowerService.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "board/IBoard.h"
#include "services/EventBus.h"
#include "services/Log.h"

using core::Rail;

static uint8_t s_ref[size_t(Rail::Count)];
static SemaphoreHandle_t s_mutex;
static Snapshot<PowerSnapshot> s_snap;
static uint32_t s_lastPollMs;

static const char* railName(Rail r)
{
    switch (r) {
    case Rail::Display: return "display";
    case Rail::Lora: return "lora";
    case Rail::AudioAmp: return "audioamp";
    case Rail::Gnss: return "gnss";
    case Rail::Nfc: return "nfc";
    case Rail::Sensor: return "sensor";
    default: return "?";
    }
}

static void adjust(Rail r, int delta)
{
    if (!s_mutex) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t& c = s_ref[size_t(r)];
    uint8_t before = c;
    if (delta > 0 && c < 255) {
        c++;
    } else if (delta < 0 && c > 0) {
        c--;
    }
    if (before == 0 && c == 1) {
        board().setRail(r, true);
        LOGI("pwr", "rail %s ON", railName(r));
    } else if (before == 1 && c == 0) {
        board().setRail(r, false);
        LOGI("pwr", "rail %s OFF", railName(r));
    }
    xSemaphoreGive(s_mutex);
}

RailLease::RailLease(Rail r) : rail_(r), held_(true) { adjust(r, +1); }
RailLease::~RailLease()
{
    if (held_) {
        adjust(rail_, -1);
    }
}
RailLease::RailLease(RailLease&& o) noexcept : rail_(o.rail_), held_(o.held_) { o.held_ = false; }
RailLease& RailLease::operator=(RailLease&& o) noexcept
{
    if (this != &o) {
        if (held_) {
            adjust(rail_, -1);
        }
        rail_ = o.rail_;
        held_ = o.held_;
        o.held_ = false;
    }
    return *this;
}

namespace PowerService {

void begin()
{
    s_mutex = xSemaphoreCreateMutex();
    // FR-017: no feature needs these in S0, so they start OFF. Display and Sensor stay on (touch/IMU).
    const Rail optional[] = {Rail::Lora, Rail::AudioAmp, Rail::Gnss, Rail::Nfc};
    for (Rail r : optional) {
        board().setRail(r, false);
        LOGI("pwr", "rail %s OFF (boot)", railName(r));
    }
}

const Snapshot<PowerSnapshot>& power() { return s_snap; }

void enforceIdleRails()
{
    const Rail optional[] = {Rail::Lora, Rail::AudioAmp, Rail::Gnss, Rail::Nfc};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (Rail r : optional) {
        if (s_ref[size_t(r)] == 0) {
            board().setRail(r, false);
        }
    }
    xSemaphoreGive(s_mutex);
    LOGI("pwr", "idle rails enforced OFF");
}

uint8_t refcount(Rail r) { return s_ref[size_t(r)]; }

void tick(uint32_t nowMs)
{
    if (nowMs - s_lastPollMs < 1000) {
        return;
    }
    s_lastPollMs = nowMs;
    PowerSnapshot p{};
    p.valid = board().readPmu(p.pmu);
    p.atMs = nowMs;
    s_snap.write(p);
    EventBus::publish(EventType::PowerSnapshotUpdated);
}

} // namespace PowerService
