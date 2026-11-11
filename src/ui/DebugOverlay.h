// FPS / heap diagnostics overlay (FR-020) and `ad stat` / `ad log` commands. UI task.
#pragma once
#include <cstdint>
#include "services/Snapshot.h"

struct DiagSnapshot {
    uint16_t fps;
    uint32_t heapFree;
    uint32_t heapMin;
    uint32_t psramFree;
};

namespace DebugOverlay {
void begin();
void setVisible(bool on);
const Snapshot<DiagSnapshot>& diag();
uint32_t frameCount();   // monotonic count of rendered frames (UI task)
void registerCommands();
} // namespace DebugOverlay
