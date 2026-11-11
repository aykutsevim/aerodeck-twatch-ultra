# Implementation Plan: Alert Manager & Haptics (Slice S4)

**Branch**: `005-alert-haptics` | **Date**: 2026-11-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/005-alert-haptics/spec.md`

**Implements**:
- AD-SAS-001 §5.2 (events `AlertRaised/Cleared`);
- §5.3 (low-battery sequence; audio deferred to S5);
- §6 (`AlertManager`, `HapticService`; core `alert_arbiter` = `core::AlertSet`/`AlertTable`);
- §7.1 (settings: haptic level, quiet);
- §8 (Annunciator widget).

It replaces the S0 `Cautions` list and `CautionBanner`, and the S1 LOW FUEL caution.

## Summary

A pure core decides everything that can be tested off-device:
- `core::AlertSet` (deterministic order by level then raise sequence, acknowledge and re-arm rules);
- `core::AlertTable` (id → level, pattern, repeat, voice-clip slot for S5);
- `core::BatteryAlarm` (STALL <15 %, PULL UP ≤5 %, hysteresis 17 % / 7 %, charging clears);
- `core::HapticPolicy` (effect sequences per intensity, quiet rules, priority).

The services wire that core to the hardware and the UI:
- **AlertManager** (svc task) owns the set, publishes a snapshot and events, and repeats the stick shaker every 2 s
  while a WARNING is unacknowledged.
- **HapticService** drives the DRV2605 through a new `IBoard::haptic(sequence)`, with priority so clicks never cut a
  warning.
- **AnnunciatorBar** (top layer) replaces the S0 banner: it shows the master alert in its level colour with a count,
  flashes until acknowledged, and a tap acknowledges.
- **Clicks** fire on page changes and control activation.
- **Settings** gain Vibration (off/low/high) and Quiet mode. All existing caution sources move to AlertManager.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests use WinLibs GCC 16.2.

**Primary Dependencies**: SensorLib `SensorDRV2605` (already pulled in by LilyGoLib, used only in `src/board/`). LVGL 9.6.
**No new libraries.**

**Storage**: NVS settings schema **v5** (`hapticlvl`, `quiet`, additive).

**Testing**:
- Unity on `env:native`:
  - `test_alerts`: AlertSet ordering/ack/re-arm/capacity, table lookups;
  - `test_battery_alarm`: thresholds, hysteresis, charging, invalid readings, sweep sequences;
  - `test_haptic_policy`: intensity × quiet × pattern matrix, priority;
  - `test_settings` v5.
- On-device: quickstart Q2–Q7. The haptic feel needs the wearer.

**Target Platform**: T-Watch Ultra (DRV2605 + ERM). The T-Watch S3 must compile and has the same chip (Cap::Haptics).

**Project Type**: Embedded firmware.

**Performance Goals**:
- click ≤50 ms after the page settles;
- band update ≤1 s;
- acknowledge stops vibration ≤1 s;
- PFD median ≥28 fps with a flashing WARNING (SC-006).

**Constraints**:
- I2C only under the I2C lock (constitution II);
- no LVGL in services;
- haptic writes never on the UI task except the click itself (≈0.5 ms);
- internal heap ≥40 KB.

**Scale/Scope**:
- 4 core modules;
- 2 new services (AlertManager, HapticService) + PowerService/TimeService/NetService/main retrofit;
- 1 UI widget (replaces CautionBanner);
- Settings section;
- ~6 serial commands;
- IBoard +2 methods.

No NEEDS CLARIFICATION remains (research R1–R8). The effect ids are **tuned on the device by feel** (R2, Q7).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.2)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | A missing haptic driver → visual alerts plus a HAPTICS FAIL caution, never a crash. The band replaces the S0 banner one-to-one, so every S0–S3 caution is still visible |
| II | Layered architecture | ✅ | ✅ | DRV2605 only in `src/board/` behind `IBoard::haptic/hapticStop` (I2C lock held). Alert logic is pure `core/`. Services have no LVGL. `AnnunciatorBar` and the click helper are in `src/ui/`. Pages talk to `AlertManager`/`HapticService` APIs only |
| III | Battery is a feature | ✅ | ✅ | **Duty cycle:** an unacknowledged WARNING ≈ 1.5 s of motor per 2 s (bounded by acknowledgement); a click ≈ 30 ms per interaction; zero when idle. The STALL warning itself is a battery-protection feature. No new rail |
| IV | Frame time is sacred | ✅ | ✅ | Pattern timing runs on the svc task. The UI only issues the one click write (≈0.5 ms). The band redraws at ≤2 Hz in a small area. FPS measured with a flashing WARNING (Q6) |
| V | Test off-device first | ✅ | ✅ | AlertSet, AlertTable, BatteryAlarm and HapticPolicy are tested first. On-device checks Q2–Q7 come from the acceptance scenarios |
| VI | Deliberate, compliant, private | ✅ | ✅ | Acknowledge is a deliberate tap on a dedicated band, consumed (never passed through). Nothing transmits. Alerts contain no personal data |
| VII | Reproducible, portable | ✅ | ✅ | No new dependencies (SensorLib is already pinned via LilyGoLib). The S3 board implements the same IBoard methods |
| VIII | Simplicity | ✅ | ✅ | One alert path (the S0 `Cautions` list and banner are removed, not kept beside). Hardware-timed sequences instead of a software PWM loop. Per-alert acknowledgement and alert history are out of scope |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments in this branch**:
- FR-HAP-002 note: PULL UP ≤5 % (haptic in S4, voice in S5), hysteresis 2 %;
- FR-HAP-003 note: warnings always vibrate at full strength;
- FR-ALR-004 note: quiet mode keeps UI clicks;
- S4 traceability row unchanged (FR-ALR-001…004, FR-HAP-001…003).

**SAS notes**:
- §6 `HapticService` row (IBoard::haptic sequence, priority);
- §8 Annunciator replaces the S0 caution banner;
- §5.2 `CautionChanged` retired.

**ADR**: none. `IBoard::haptic` is the SAS-named hook, extended to a sequence.

## Project Structure

### Documentation (this feature)

```text
specs/005-alert-haptics/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/{serial_commands.md, annunciator_ui.md}
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/
├── Alerts.{h,cpp}          # AlertLevel, AlertId, AlertDef table, alertDef(), names
├── AlertSet.{h,cpp}        # active set: raise/clear/ackAll/sorted (pure)
├── BatteryAlarm.{h,cpp}    # STALL/PULL UP state machine with hysteresis (pure)
├── HapticPolicy.{h,cpp}    # Pattern/Intensity → effect sequence, quiet, priority (pure)
└── SettingDefs.{h,cpp}     # + hapticlvl, quiet; schema 5
src/board/
├── IBoard.h                # + haptic(seq,n), hapticStop()
├── BoardUltra.cpp          # implement via instance.drv (I2C lock)
└── BoardS3.cpp             # same
src/services/
├── AlertManager.{h,cpp}    # set + snapshot + events + repeat; `ad alert ...`
├── HapticService.{h,cpp}   # play/click with priority; `ad haptic ...`
├── PowerService.cpp        # BatteryAlarm → STALL/PULL UP; `ad fuel sim <pct> [chg|nochg]`
├── TimeService.cpp, NetService.cpp   # Cautions → AlertManager
├── EventBus.h              # + AlertRaised/AlertCleared/AlertAcked; − CautionChanged
└── Cautions.{h,cpp}        # removed
src/ui/
├── AnnunciatorBar.{h,cpp}  # replaces CautionBanner.{h,cpp} (removed)
├── HapticClick.{h,cpp}     # attachClick(lv_obj_t*) helper
└── PageManager.cpp         # click on page settled
src/pages/SettingsPage.cpp, WxPage.cpp   # ALERTS section; attachClick on controls
src/app/main.cpp            # boot cautions → AlertManager; begin order
test/
├── test_alerts/  test_battery_alarm/  test_haptic_policy/
└── test_settings/          (extended: v5)
docs/software_requirements_specification.md, docs/software_architecture_specification.md, README.md
```

**Structure Decision**: Continue the S0–S3 layout.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. Core tests (AlertSet, table, BatteryAlarm, HapticPolicy, settings v5) → core implementation.
   2. IBoard haptic + HapticService + `ad haptic`.
   3. **Checkpoint on the device:** feel each pattern; measure the GO-bit pattern lengths.
   4. AlertManager + `ad alert`, then the retrofit of all `Cautions` users (remove `Cautions`).
   5. AnnunciatorBar (remove CautionBanner).
   6. BatteryAlarm in PowerService + `fuel sim … nochg`.
   7. Clicks (PageManager, attachClick).
   8. Settings ALERTS section.
   9. Docs, validation.
2. **Repeat timing:** the AlertManager svc tick (50 ms) checks `anyUnackedWarning` and the last shaker start;
   it re-plays at +2000 ms. Acknowledge → `HapticService::stop(prio 4)` so the vibration stops ≤1 s (FR-007).
3. **Haptic priority state** (`curPrio`, `curEndMs`) lives in HapticService behind a small mutex. `click()` from the
   UI task calls `board().haptic()` directly (I2C lock); `play()` from the svc task does the same.
4. **Boot:** the battery alarm runs from PowerService's first valid poll, after the face. Boot cautions are raised
   through AlertManager in `raiseBootCautions`. LilyGoLib's boot buzz (effect 15) is unchanged.
5. **Measure (Q6):** PFD FPS median with `stall` unacknowledged and the sweep on; heap. Record in `validation.md`.
6. **At the end:** update `docs/product_roadmap.md` per the status tracking rule (S4 → ✅ after merge; M2 stays 🚧
   until S5 and S6).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
