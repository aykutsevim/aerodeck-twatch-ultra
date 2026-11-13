# ADR-014: Deep sleep without LilyGoLib's `sleep()`; button wake only (closes SRS OI-1)

- **Status:** Accepted (slice S7, 2026-11-13)
- **Relates to:** constitution Principles II (layering), III (battery), VIII (time-boxed experiments);
  AD-SAS-001 §4, §5.3; SRS FR-EJT-002/003, OI-1; spec `specs/008-nvg-eject-power` (research R6/R7)

## Context

S7's Pull-to-Eject must reach deep sleep **within 2 s**, keep the data, and wake on the button (and on a wrist
movement if the hardware can). LilyGoLib 0.2.0 has `LilyGoUltra::sleep()`, but:

1. It waits **2 s** (`delay(500)` × 4, for its own log) before `esp_deep_sleep_start()`. That alone breaks the
   budget.
2. It enables **either** the timer **or** the wake pins, never both. S8's timer needs the button and the timer.
3. It powers off the sensor hub rail (`POWER_SENSOR`). Measured in S7: after a wake with the hub rail cut, the
   BHI260AP delivers **frozen** gravity/accelerometer values (samples keep arriving at 100 Hz, but the values do not
   change). The horizon, raise-to-wake and the recorder's Max G stop working until a normal reboot.
4. It cuts the RTC backup battery charge (to save about 200 µA).

## Decision

`IBoard::deepSleep(wakeMask, timerSec)` in `BoardUltra` runs AeroDeck's own sequence, built from LilyGoLib's
**public** API. No library patch is needed.

1. Wait for the asynchronous display flush to finish (S7 flush task).
2. PMU: disable all IRQs, enable `PKEY_SHORT` for the button.
3. Peripherals:
   - touch sleep;
   - display `LilyGoDispQSPI::sleep()` + `end()`;
   - `uninstallSD()`;
   - expander pins `DRV_EN`/`DISP_EN` low;
   - `disablePowerMeasure()`, `pmu.enableSleep()`.
4. Power off haptics, GPS, speaker, NFC, SD and radio.
   - The **display rail stays on**: LilyGoLib measured about +600 µA when it is cut; the panel itself is asleep.
   - The **sensor hub stays powered but idle**: its gravity, accelerometer and step-counter virtual sensors are
     stopped. This fixes item 3; on wake LilyGoLib re-uploads the hub firmware as on any boot.
   - The **RTC backup charge stays on**, so the clock survives.
5. `Serial1/SPI/Wire.end()`, open-drain the unused pins (the vendor list).
6. Wake sources:
   - `esp_sleep_enable_ext1_wakeup(1 << PMU_INT, ANY_LOW)` for the button;
   - `esp_sleep_enable_timer_wakeup()` when a pre-sleep hook asked for a time.
7. `esp_deep_sleep_start()`.

**Orchestration:** `PowerService::eject()` on the svc task.
- It saves the recorder, stops audio, haptics and the setup portal, and runs the pre-sleep hooks (S8).
- It quiesces the UI task (a flag and a semaphore: the UI stops LVGL and turns the panel off), then calls
  `deepSleep`.
- It is the **only** caller of `deepSleep`. The rail leases (constitution III) become void because the chip resets
  on wake.

**Wake cause:** read at boot from `esp_sleep_get_wakeup_cause()` + the ext1 status, and logged
(`[boot] wake=button`). `ad wake` shows it with the last eject's timing (kept in `RTC_NOINIT` memory).

**State across sleep:** the recorder is saved to NVS at eject. No `RTC_DATA_ATTR` mirror is added: NVS is enough,
and RTC memory does not survive a power loss.

## Wrist-motion wake (SRS OI-1) — spike result: not promoted

- **Setup:** time-boxed spike behind `-DAERODECK_IMU_WAKE` (default off).
  - The BHI260AP interrupt is routed to `SENSOR_INT` = GPIO8, an RTC GPIO, so ext1 wake is possible in principle.
  - The loaded KLIO firmware **offers** the wake-up sensors wrist tilt (67), wake gesture (57) and significant
    motion (55), checked at runtime.
  - Before sleep the spike set the hub interrupt active-low with only the wake-up FIFO enabled
    (`DISABLE_FIFO_NW | DISABLE_STATUS_FIFO | DISABLE_DEBUG | ACTIVE_LOW`), armed the sensor at 1 Hz, and added
    GPIO8 to the ext1 mask.
- **Result:**
  - wrist tilt (67): no wake in 3 deliberate tilts;
  - significant motion (55): no wake in about 20 s of walking and swinging.
  - The chip woke only from the button.
  - The cause (interrupt routing, polarity in sleep, or the hub not raising INT without a host read) was not found
    within the time box.
- **Decision:** button-only wake, which is the SRS OI-1 fallback. `Cap::ImuWakeInterrupt` stays unset. The spike
  code remains behind the flag for a later attempt. S8's timer wake does not depend on it.

## Consequences

- Eject is fast: 3–14 ms from the request to the sleep call (console); **103 ms** from the end of the gesture.
- The sensor hub's idle current is part of the ejected state. Measured with a USB meter: the whole ejected watch
  is below the meter's 1 mA resolution.
- Steps walked while ejected are not counted: the hub firmware is re-uploaded at boot and its counter restarts
  (S6's `StepAccumulator` handles that).
- A wake from deep sleep is a full boot: about 3.4 s to the watch face (`ad boot`), within FR-PLT-001's 4 s.
- The button press that wakes the chip is cleared from the PMU IRQ status at boot, so it is not delivered again as
  a click.

## Alternatives

- **Patch out LilyGoLib's delay (`AERODECK-PATCH`):** still leaves timer XOR pins and the sensor rail cut. Rejected.
- **Periodic timer wake to poll the IMU:** each wake is a full boot (about 3.5 s at about 40 mA), worse than staying
  awake. Rejected.
- **Cut the sensor hub rail and re-initialise it differently on wake:** possible, but keeping the hub powered and
  idle works and is simpler. Revisit if the hub's sleep current turns out to matter.
