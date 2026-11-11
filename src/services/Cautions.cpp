#include "services/Cautions.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include "services/EventBus.h"
#include "services/Log.h"

namespace Cautions {

static Caution s_items[kMax];
static size_t s_n;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void raise(CautionId id, const char* text)
{
    bool added = false;
    portENTER_CRITICAL(&s_lock);
    size_t i = 0;
    for (; i < s_n; i++) {
        if (s_items[i].id == id) {
            break;
        }
    }
    if (i == s_n && s_n < kMax) {
        s_n++;
        added = true;
    }
    if (i < kMax) {
        s_items[i].id = id;
        strncpy(s_items[i].text, text ? text : "", sizeof s_items[i].text - 1);
        s_items[i].text[sizeof s_items[i].text - 1] = '\0';
    }
    portEXIT_CRITICAL(&s_lock);
    if (added) {
        LOGW("caution", "%s", text ? text : "");
        EventBus::publish(EventType::CautionChanged);
    }
}

void clear(CautionId id)
{
    bool removed = false;
    portENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < s_n; i++) {
        if (s_items[i].id == id) {
            s_items[i] = s_items[--s_n];
            removed = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_lock);
    if (removed) {
        EventBus::publish(EventType::CautionChanged);
    }
}

size_t active(Caution* out, size_t max)
{
    portENTER_CRITICAL(&s_lock);
    size_t n = s_n < max ? s_n : max;
    for (size_t i = 0; i < n; i++) {
        out[i] = s_items[i];
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

} // namespace Cautions
