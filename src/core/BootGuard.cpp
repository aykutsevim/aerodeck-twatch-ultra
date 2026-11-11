#include "core/BootGuard.h"

namespace core {
namespace BootGuard {

BootMode onBoot(BootRecord& r, bool abnormalReset)
{
    if (r.magic != kMagic) {
        r = BootRecord{kMagic, 0, BootStage::Start, false};
        return BootMode::Normal;
    }
    BootStage crashedAt = r.lastStage;
    if (abnormalReset && crashedAt != BootStage::Stable) {
        if (r.fastFailCount < 255) {
            r.fastFailCount++;
        }
    } else {
        r.fastFailCount = 0;
    }
    r.lastStage = BootStage::Start;

    BootMode m = BootMode::Normal;
    if (r.fastFailCount >= kHeadlessAfter ||
        (r.fastFailCount >= kSafeAfter && crashedAt == BootStage::DisplayInit)) {
        m = BootMode::Headless;
    } else if (r.fastFailCount >= kSafeAfter) {
        m = BootMode::Safe;
    }
    r.safeMode = (m != BootMode::Normal);
    return m;
}

void onStable(BootRecord& r)
{
    r.fastFailCount = 0;
    r.lastStage = BootStage::Stable;
}

} // namespace BootGuard
} // namespace core
