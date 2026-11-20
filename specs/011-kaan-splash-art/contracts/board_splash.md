# Contract: board splash API (`IBoard`)

```cpp
#include "core/AlphaArt.h"   // struct AlphaArt { uint16_t w, h; const uint8_t* a8; };
struct SplashSpec {
    const AlphaArt* art = nullptr;   // nullptr = no splash
    uint16_t fg565 = 0xFFFF;         // day white; NVG: core::NvgColor::toGreen565(0xFFFF)
    uint8_t brightnessPct = 80;      // the same value applyBrightness() will set
};
enum class SplashSkip : uint8_t { None, Wake, Safe, NoArt, NoMem };   // defined in core/SplashCompose.h
struct SplashResult { bool shown; uint32_t atMs; uint32_t costMs; SplashSkip skip; };

virtual void setSplash(const SplashSpec&) {}                 // call before begin(); default: no splash
virtual SplashResult splashResult() const { return {false, 0, 0, SplashSkip::NoArt}; }
```

## BoardUltra behaviour (as implemented; research R2 revision)

1. In `begin()`, before `instance.begin()`, if the show rule holds (`art`, mode Normal, `wakeCause == PowerOn`):
   - allocate a full-screen buffer (410 × 502 RGB565, 411 KB) in **PSRAM**;
   - start a core-0 task that composes it with `core::SplashCompose::row` (byte-swapped), then calls
     `instance.setBootImage(buf)`. The composition runs in parallel with LilyGoLib's PMU and panel reset (it takes
     ≈ 57 ms; the push comes ≈ 1.4 s later). If the image were not ready in time, LilyGoLib would push nothing.

   Otherwise `skip` = `wake | safe | none`, or `nomem` if the buffer cannot be allocated.
2. `instance.begin(flags)` is unchanged (LilyGoLib's init order is kept). LilyGoLib pushes the boot image right
   after the panel init, before the 32 kHz crystal calibration and the touch/RTC/audio init.
   **AERODECK-PATCH-3** (`tools/patch_lilygolib.py`) replaces its synchronous 5 s `incrementalBrightness(250, 20)`
   with `aerodeck_boot_image_shown()` (a weak empty default in LilyGoLib).
3. BoardUltra's `aerodeck_boot_image_shown()` waits **2 ms** (the SPI ISR must finish the DMA transaction first;
   validation T014), sets the brightness to `brightnessPct` and records `atMs`.
4. After `instance.begin()`: wait until the composition is done (not expected to wait), then
   `setBootImage(nullptr)`, free the buffer, and fill `SplashResult`. The splash stays on the panel until LVGL's
   first PFD frame overwrites it.

`main` logs `[boot] splash shown at <atMs> ms (<costMs> ms)` / `[boot] splash skipped (<wake|safe|none|nomem>)` (the
board layer does not log).

**Related runtime guard:** `waitFlushIdle()` (before brightness, screen off/on and deep sleep) waits 2 ms after the
flush task goes idle, for the same SPI bus-lock reason.

**Errors:** no PSRAM, or the image not ready → no splash; the boot continues normally (FR-005).

## BoardS3

Inherits the no-op defaults (research R8). Patch 3 is applied to the S3 env's copy of `LilyGoWatchUltra.cpp` too;
the weak hook keeps it unchanged in behaviour.
