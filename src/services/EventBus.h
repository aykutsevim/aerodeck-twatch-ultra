// AeroDeck EventBus: fixed-size events fanned out to per-subscriber FreeRTOS queues (data-model §8).
#pragma once
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class EventType : uint16_t {
    PageChanged = 1,          // a = from PageId, b = to PageId
    SettingsChanged,          // a = SettingKey
    CautionChanged,
    ButtonClick,
    PowerSnapshotUpdated,
    PeriphStatusChanged,      // deferred probe finished
    NavCommand,               // a = 0 next, 1 prev, 2 button (serial test hook `ad nav`)
    AttitudeStatusChanged,    // S1: Align → Ok/Fail
    ClockChanged,             // S1: clock set via serial
};

struct Event {
    EventType type;
    uint16_t pad;
    uint32_t a;
    uint32_t b;
};

namespace EventBus {
constexpr int kMaxSubscribers = 8;
QueueHandle_t subscribe(uint8_t depth);
void publish(const Event& e);   // non-blocking; drops (and counts) when a queue is full
inline void publish(EventType t, uint32_t a = 0, uint32_t b = 0) { publish(Event{t, 0, a, b}); }
uint32_t dropped();
} // namespace EventBus
