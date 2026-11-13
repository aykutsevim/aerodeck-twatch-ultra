# Contract: IBoard Additions (S7)

Extends `specs/001-platform-foundation/contracts/internal_interfaces.md`. Only `src/board/` implements these.

```cpp
enum class WakeCause : uint8_t { PowerOn, Button, Imu, Timer, Other };   // core/Types.h

// UI task only.
// false: AMOLED sleep-in + panel enable low (rail kept on).
// true: wake + panel enable high.
// Takes the I2C lock (expander).
virtual void setDisplayOn(bool on) = 0;

// UI task (main.cpp applySetting). While on, every flushed buffer goes through core::NvgColor::apply
// before pushColors. Cheap toggle (a flag read in the flush wrapper).
virtual void setNvgFilter(bool on) = 0;

// svc task, last call of PowerService::eject(), I2C lock held by the caller.
// Never returns on success. Returns false when deep sleep is unsupported on this board.
// wakeMask: bit 0 button, bit 1 IMU (only if caps() has ImuWakeInterrupt), bit 2 timer.
// timerSec > 0 adds a timer wake.
// Sequence: research R6. No vendor sleep() (its 2 s delay breaks FR-008). The RTC backup is kept charging.
virtual bool deepSleep(uint8_t wakeMask, uint32_t timerSec) = 0;

// Valid from boot. Mapped from esp_sleep_get_wakeup_cause() + ext1 status.
virtual WakeCause wakeCause() const = 0;
```

`setBrightnessPct` is unchanged. NVG calls it with 10.

**Caps:** `Cap::ImuWakeInterrupt` is set on the Ultra only when `AERODECK_IMU_WAKE` is promoted (research R7).

**T-Watch S3:**
- `setDisplayOn`: LilyGoLib backlight off/on (`instance.setBrightness(0)` / restore).
- `setNvgFilter`: same shared flush wrapper.
- `deepSleep`: the same ESP-IDF sequence with the S3 PMU pin (`PMU_INT` 21); returns false if not validated.
- `wakeCause`: same mapping.
