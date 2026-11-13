#include "core/NvgStyle.h"

namespace core {
namespace NvgStyle {

AlertLook forLevel(AlertLevel lvl, bool nvg, uint32_t nowMs)
{
    AlertLook k{false, false, true};
    if (!nvg) {
        return k;
    }
    switch (lvl) {
    case AlertLevel::Warning:
        k.inverse = true;
        k.visible = (nowMs / kBlinkHalfMs) % 2 == 0;
        break;
    case AlertLevel::Caution:
        k.inverse = true;
        break;
    default:
        k.outline = true;
        break;
    }
    return k;
}

} // namespace NvgStyle
} // namespace core
