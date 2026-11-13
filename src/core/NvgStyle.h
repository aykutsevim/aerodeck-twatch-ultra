// AeroDeck core: alert looks in NVG (S7, FR-015, research R2, data-model §6). Pure; host-tested.
// Day: normal colours. NVG: WARNING inverse + 2 Hz blink, CAUTION inverse steady, ADVISORY green outline on black.
#pragma once
#include <cstdint>
#include "core/Alerts.h"

namespace core {
namespace NvgStyle {

struct AlertLook {
    bool inverse;   // green background, black text
    bool outline;   // black background, green border and text
    bool visible;   // blink phase (false = draw the bar dark this instant)
};

constexpr uint32_t kBlinkHalfMs = 250;

AlertLook forLevel(AlertLevel lvl, bool nvg, uint32_t nowMs);

} // namespace NvgStyle
} // namespace core
