#include "services/SvcTask.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "services/Log.h"

namespace SvcTask {

static TickFn s_ticks[kMaxTicks];
static int s_n;

bool addTick(TickFn fn)
{
    if (!fn || s_n >= kMaxTicks) {
        return false;
    }
    s_ticks[s_n++] = fn;
    return true;
}

static void run(void*)
{
    esp_task_wdt_add(nullptr);   // FR-016: a hung service task resets the chip (5 s timeout)
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t now = millis();
        for (int i = 0; i < s_n; i++) {
            s_ticks[i](now);
        }
        esp_task_wdt_reset();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(kPeriodMs));
    }
}

void begin()
{
    // ESP-IDF stack depth is in bytes.
    xTaskCreatePinnedToCore(run, "svc", 8192, nullptr, 2, nullptr, 0);
    LOGI("svc", "service task started (core 0, %d ticks)", s_n);
}

} // namespace SvcTask
