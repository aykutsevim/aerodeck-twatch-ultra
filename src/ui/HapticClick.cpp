#include "ui/HapticClick.h"
#include "services/HapticService.h"

namespace HapticClick {

static void clickCb(lv_event_t*) { HapticService::click(); }

void attach(lv_obj_t* obj, bool onValueChanged)
{
    if (obj) {
        lv_obj_add_event_cb(obj, clickCb, onValueChanged ? LV_EVENT_VALUE_CHANGED : LV_EVENT_CLICKED, nullptr);
    }
}

} // namespace HapticClick
