#include "services/EventBus.h"
#include <freertos/semphr.h>

namespace EventBus {

static QueueHandle_t s_subs[kMaxSubscribers];
static int s_count;
static volatile uint32_t s_dropped;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

QueueHandle_t subscribe(uint8_t depth)
{
    QueueHandle_t q = xQueueCreate(depth, sizeof(Event));
    if (!q) {
        return nullptr;
    }
    portENTER_CRITICAL(&s_lock);
    if (s_count >= kMaxSubscribers) {
        portEXIT_CRITICAL(&s_lock);
        vQueueDelete(q);
        return nullptr;
    }
    s_subs[s_count++] = q;
    portEXIT_CRITICAL(&s_lock);
    return q;
}

void publish(const Event& e)
{
    int n = s_count;   // subscribers are only appended, never removed
    for (int i = 0; i < n; i++) {
        if (xQueueSend(s_subs[i], &e, 0) != pdTRUE) {
            s_dropped++;
        }
    }
}

uint32_t dropped() { return s_dropped; }

} // namespace EventBus
