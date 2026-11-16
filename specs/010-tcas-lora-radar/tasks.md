---

description: "Task list for TCAS LoRa Radar (Slice S9)"
---

# Tasks: TCAS LoRa Radar (Slice S9)

**Input**: Design documents from `/specs/010-tcas-lora-radar/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/board_radio.md,
contracts/serial_commands.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V and spec FR-012 require Unity tests for the new `src/core/` modules,
written before the implementation.

**Organization**:
- US1: see nearby watches on a radar (P1)
- US2: inspect a contact (P1)
- US3: broadcast my own beacon, legally and frugally (P1)
- US4: TRAFFIC advisory (P2)

**One device only** (wearer, 2026-11-16). Receiving from a second device is replaced by simulated contacts and a
listen-only receive probe (research R8b). SC-001, SC-002 and the real-device part of SC-005 stay *pending manual*.

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`.
  - Native tests need WinLibs GCC on PATH. Unity has no double asserts: use float or int asserts.
  - Serial tools open COM3 with **DTR/RTS low**. USB CDC drops lines, so cross-check with `ad tcas` and
    `ad pwr log`.
  - Deep sleep removes COM3; scripts reopen it (scratchpad `serlog.py`, `adser.py`).
- **Rules from S0–S8 still hold:**
  - RadioLib / LilyGoLib / SensorLib only in `src/board/`;
  - pure `src/core/`;
  - no LVGL in services;
  - display power changes only on the UI task;
  - one writer per Snapshot.
- **Write helper scripts and C string edits with the Write/Edit tools** (shell heredocs mangle escapes; never
  round-trip files through PowerShell `Get-Content`/`Set-Content`).
- **RF legality:** only **EU868** is used on the device (Türkiye). Never select US915 on the device; US915 is
  tested off-device only. Never transmit outside the region table. The receive probe is listen-only.
- **Never run `ad set reset` on the device without asking.** Never print or change the Wi-Fi credentials.
- **Wearer checks:** ask, and record the answer verbatim in `validation.md`.
- **References:** `data-model.md` §n, `research.md` R1–R8b, `quickstart.md` Q0–Q7, `contracts/board_radio.md`,
  `contracts/serial_commands.md`.

---

## Phase 1: Setup

- [X] T001 Create `test/test_beacon_codec/`, `test/test_duty_cycle/`, `test/test_lora_airtime/`,
  `test/test_tcas_table/` and `test/test_lora_region/`. Verify the baseline on branch `010-tcas-lora-radar`:
  - `pio test -e native` (269/269);
  - both builds (Ultra, S3).

  Create `specs/010-tcas-lora-radar/validation.md` with the baseline (RAM, flash, PFD fps, screen-off current
  from S8: 34 mA).

---

## Phase 2: Foundational (blocking)

**Purpose:** prove the radio on this hardware first (EU868 transmit, receive, rail), then add the pure core
modules, the alert and settings v10.

- [X] T002 Add the radio API to `src/board/IBoard.h` exactly as in `contracts/board_radio.md`:
  - `RadioProfile`, `RadioPacket`;
  - `radioStart`, `radioStop`, `radioSend`, `radioPoll`, `radioWait`, `radioAirtimeMs`, `nodeId`.

  Implement the stubs in `src/board/BoardS3.cpp`: `radioStart` → false, the others are no-ops / false, and
  `nodeId` = the low 32 bits of `ESP.getEfuseMac()`.
- [X] T003 Implement the radio API in `src/board/BoardUltra.cpp` with LilyGoLib's global `SX1262 radio` (research
  R2):
  - every call under `instance.lockSPI()` / `unlockSPI()`;
  - `radioStart`:
    - `radio.begin(freq, bw, sf, cr, syncWord, power, preamble)`;
    - `instance.setRFSwitch(false)` (built-in antenna);
    - `setCurrentLimit(140)`;
    - `setDio1Action(isr)`, where the ISR only gives a task notification / binary semaphore (no SPI in the ISR);
    - `startReceive()`;
    - log the RadioLib error code on failure and return false;
  - `radioStop`: `clearDio1Action()`, `radio.sleep()`;
  - `radioSend`:
    - `transmit()` (blocking);
    - `airMs` = the measured `millis()` delta;
    - then `startReceive()` again;
  - `radioPoll`:
    - if the DIO1 flag is set: `getPacketLength`, `readData`, `getRSSI`, `getSNR` (×4 → `snrQ4`);
    - `crcError` when `readData` returns `RADIOLIB_ERR_CRC_MISMATCH`;
    - then `startReceive()`;
  - `radioWait(timeout)` takes the semaphore;
  - `radioAirtimeMs(n)` = `radio.getTimeOnAir(n) / 1000`;
  - `nodeId` = the low 32 bits of the eFuse MAC.

  The rail is **not** switched here; the caller holds `RailLease(Lora)`. `radioStart` re-runs `begin` each time,
  because the chip loses its configuration when the rail is off.
- [X] T004 Add the test hooks `ad probe s9 tx` and `ad probe s9 rx <s>` to `src/services/BoardCommands.cpp`
  (`contracts/serial_commands.md`):
  - both take `RailLease(Lora)`, call `radioStart` with the EU868 default profile (869.525 MHz, SF9, BW125, CR4/5,
    10 dBm, sync 0x12, preamble 8), and release it at the end;
  - `tx`: 3 × an 18-byte test packet 2 s apart; print OK/fail, the measured airtime, `radioAirtimeMs`;
  - `rx <s>` (max 120): sync word **0x34**, listen only, count packets, CRC errors and RSSI min/max.
  - **On-device Q0 (steps 1–4):**
    - 3 × transmit-done, airtime ≈ 185 ms ±10 %;
    - the receive probe runs (zero packets is acceptable and noted);
    - `ad pwr log` shows `lora:1` during the probe and `lora:0` after;
    - an SD listing during the probe has no SPI errors.
  - Record in `validation.md`. **If the radio does not start or transmit, stop and revise R2** before
    continuing.
- [X] T005 [P] Write `test/test_beacon_codec/test_main.cpp` for `core::BeaconCodec` (data-model §3):
  - encode → 18 bytes, the exact SAS §7.2 byte layout (little-endian `nodeId`, the callsign padded with spaces);
  - round trip of every field; the 24-byte position variant round trip;
  - rejects:
    - length ≠ 18/24;
    - a wrong magic or version;
    - an unknown type;
    - a non-printable callsign;
    - battery > 100;
    - a has-position flag / length mismatch;
  - a lowercase callsign is uppercased;
  - the output is untouched on failure;
  - `isMeshtastic` → false.
- [X] T006 [P] Write `test/test_lora_airtime/test_main.cpp` for `core::LoraAirtime` (data-model §4):
  - SF9/BW125/CR5/preamble 8/18 B explicit header + CRC ≈ 185 ms (±2);
  - SF7/BW125 18 B ≈ 51 ms;
  - SF12/BW125 18 B ≈ 1155 ms (low-data-rate optimisation on);
  - SF9/BW250 is about half of BW125;
  - airtime grows monotonically with the payload.
- [X] T007 [P] Write `test/test_duty_cycle/test_main.cpp` for `core::DutyCycle` (data-model §5):
  - empty → `canSend`;
  - the sum within the window;
  - records older than 1 h drop out;
  - the exact budget boundary (= allowed, +1 ms refused);
  - `nextAllowedMs` is correct;
  - `save` → `restore` round trip keeps `usedMs`; a bad magic or CRC → false and empty; a record dated in the future
    is clamped to now (UTC ms, int64);
  - **soak simulation:** 1 h at a 5 s interval with 185 ms beacons, gated by `canSend`: the used time never exceeds
    36 000 ms in any rolling hour, and about 194 beacons are sent;
  - a 256-record ring overflow stays conservative.
- [X] T008 [P] Write `test/test_tcas_table/test_main.cpp` for `core::TcasTable` (data-model §6):
  - `ringFor`:
    - −30 → 0, −40 → 0, −55 → 1, −70 → 2, −85 → 3, −120 → 3;
  - `bearingFor` is stable and in 0..359; distinct ids spread out;
  - the EMA (α 0.3): a new contact = rssi; the next update moves 30 %;
  - the update keeps the callsign, battery and SNR, and the last-seen time;
  - `enteredInner`:
    - new at −40 → true;
    - staying → false;
    - leaving to ring ≥ 1 then back → true again;
    - aged out then back → true;
  - `age` removes stale contacts only;
  - a full table of 16 + a new contact replaces the oldest;
  - simulated contacts are flagged.
- [X] T009 [P] Write `test/test_lora_region/test_main.cpp` for `core::LoraRegion` (data-model §2):
  - EU868 default profile = 869.525 MHz / 10 dBm / SF9 / BW125;
  - power 20 → clamped to 14, 0 → 2;
  - SF outside 7..12 clamped;
  - a BW index outside the list → 0;
  - US915: power ≤ 20, `maxDwellMs` 400 → an SF12/BW125 profile is reported as exceeding the dwell (the caller
    must refuse it);
  - the clamp report flags each clamped field.
- [X] T010 [P] Extend `test/test_settings/test_main.cpp` for schema 10 (data-model §1):
  - `tcas` default 0;
  - `callsign` 1..8 `[A-Z0-9-]` (rejects 9 characters and invalid characters);
  - `tcasint` 15..300 default 30;
  - `tcasage` 60..1800 default 300;
  - `lorareg` 0..1;
  - `lorapwr` 2..20 default 10;
  - `lorasf` 7..12 default 9;
  - `lorabw` 0..2 default 0;
  - migration v9 → v10.

  Extend the alert tests (the existing core alert test):
  - id 44 `traffic`, Advisory, clip `Traffic`, `alertIdFromName("traffic")`;
  - `calloutFor(Traffic, Advisory)` → `Clip::Traffic`;
  - **every other advisory still → `Clip::None`**;
  - Caution/Warning unchanged.

  Extend `test/test_screen_policy/test_main.cpp`:
  - `onAlert(Advisory, now, speaks=true)` with the screen off → turns on (cause Alert);
  - `onAlert(Advisory, now, speaks=false)` → no wake;
  - Caution/Warning unchanged.
- [X] T011 Implement `src/core/BeaconCodec.{h,cpp}` and `src/core/LoraAirtime.{h,cpp}` (pure). T005 and T006
  pass.
- [X] T012 [P] Implement `src/core/DutyCycle.{h,cpp}` (pure, a ring of 256 records, int64 UTC ms, `save`/`restore`
  with the CRC-16 from `core::RecBlob`). T007 passes.
- [X] T013 [P] Implement `src/core/TcasTable.{h,cpp}` (pure, uses `Beacon` from T011). T008 passes.
- [X] T014 [P] Implement `src/core/LoraRegion.{h,cpp}` (pure, includes the `RadioProfile` fields without depending
  on `IBoard`: a core `LoraProfile` struct that the service copies into `RadioProfile`). T009 passes.
- [X] T015 Add `Traffic` = 44 to `src/core/Alerts.{h,cpp}`:
  - the table row: Advisory, `Pattern::Advisory`, clip `Clip::Traffic`, text "TRAFFIC";
  - the name `traffic`;
  - `calloutFor`: an Advisory returns its def's explicit clip if one is set, else `None`.

  `Clip::Traffic` already exists (`src/core/Alerts.h`). The clip is queued at `SoundPrio::Caution` by the existing
  `AlertManager::callout` rule (no change).

  **Screen wake for a speaking advisory** (research R6):
  - `src/core/ScreenPolicy.{h,cpp}`: `onAlert(AlertLevel, uint32_t nowMs, bool speaks = false)`; an Advisory wakes
    only when `speaks`;
  - `src/services/AlertManager.cpp`: the `AlertRaised` event `b` = level | (speaks << 8), where speaks =
    `calloutFor(id, level) != Clip::None`;
  - `src/ui/ScreenService.{h,cpp}` and `src/app/main.cpp`: decode the level (bits 0–7) and speaks (bit 8) and pass
    them on.

  Add the eight settings to `src/core/SettingDefs.{h,cpp}` with `kSchemaVersion` = 10, the callsign validator
  and the migration. T010 passes.

**Checkpoint:** the radio is proven on the device in EU868, the core is green, and both boards build.

---

## Phase 3: User Story 3 — Broadcast my own beacon, legally and frugally (P1) 🎯 MVP

**Goal:** with TCAS on, the watch beacons every interval within the region limits and the 1 % budget; the radio
is powered only while TCAS is on.

**Independent test:**
- Q1: 10 min at 30 s → about 20 beacons, all transmit-done, jittered;
- Q2: a 1-hour soak at a 5 s interval never exceeds 36 s/h;
- `ad pwr log` shows `lora:0` with TCAS off.

US3 comes first because US1, US2 and US4 all need the running service and radio task.

- [X] T016 [US3] Create `src/services/TcasService.{h,cpp}` (research R2, R4; data-model §7):
  - **Lifecycle:**
    - `begin()`: if the `tcas` setting is 1 → `start()`;
    - `start()`:
      - take `RailLease(Lora)`;
      - resolve the profile via `core::LoraRegion` from the settings;
      - create the **`radio` task** (core 0, priority 4, 4 KB) and queues;
      - `radioStart`;
      - on failure: `radioOk = false`, release the lease, raise the existing peripheral caution;
    - `stop()`:
      - signal the task;
      - the task calls `radioStop` and exits;
      - release the lease;
    - the S3 → `radioOk = false` without a lease.
  - **Radio task loop:**
    - `radioWait(50 ms)`;
    - drain `radioPoll` into an RX queue;
    - take TX requests from a TX queue → `radioSend` → post the result (ok, `airMs`) back.
  - **Scheduler (svc tick):**
    - `nextBeaconMs` = now + interval × (0.9 + 0.2 × rand);
    - when due: `airMs` = `LoraAirtime` for 18 B;
    - if `duty.canSend` (and the US915 dwell is OK): encode the beacon (`nodeId`, `seq++`, callsign, battery % from
      `PowerService::snapshot().read().pmu.battPct`), queue it, `duty.record` on the result (the measured airtime);
    - otherwise `delayed++` and retry at `nextAllowedMs`;
    - when more than one interval late: `skipped++` and reschedule.
  - Settings changes (region, power, SF, BW, interval) restart the radio with the new profile and keep the
    `DutyCycle` history.
  - **Duty persistence (research R4):** the `DutyCycle` ring is saved to an `RTC_NOINIT_ATTR` buffer after every
    `record` and restored in `begin()` (UTC ms from the system clock), so eject and soft resets cannot reset the
    budget. Timestamps use the system clock, never `millis()`.
  - Publish `TcasSnapshot` (one writer).
  - Log lines as in `contracts/serial_commands.md`.
- [X] T017 [US3] Add the console commands in `TcasService` (`contracts/serial_commands.md`):
  - `ad tcas` (the state line + contact lines);
  - `ad tcas on|off` (writes the `tcas` setting);
  - `ad tcas beacon` (duty gate applies);
  - `ad tcas interval <s>` (temporary, 5..300).

  Settings `callsign`, `tcasint`, `tcasage`, `lorareg`, `lorapwr`, `lorasf`, `lorabw` work through the existing
  `ad set` with the region clamp, and the reply reports the clamped value.

  An empty callsign → `AD-xxxx` from `nodeId`.
- [X] T018 [US3] Wire into `src/app/main.cpp`:
  - `TcasService::begin()` after PowerService/Settings;
  - tick on the svc task;
  - stop the radio in the PowerService pre-sleep hook before eject (the board already cuts `POWER_RADIO`);
  - after the wake, `begin()` resumes if `tcas` = 1.
- [X] T019 [US3] **On-device Q1 + Q2 + Q7:**
  - Q1: 10 min at 30 s → `sent` 20 ±1, every one transmit-done, intervals 27–33 s (from the log), the beacon decode
    shows only version, id, callsign, seq and battery;
  - Q2: `ad tcas interval 5` (the test interval, below the 15 s user minimum; SC-003) for **1 hour** with a
    scratchpad script polling `ad tcas` every minute:
    - the maximum `duty` ≤ 36.0 s;
    - `delayed` / `skipped` grow;
    - no transmit while over budget;
  - Q7:
    - `ad set lorapwr 20` → 14;
    - `lorasf 12` accepted and the airtime used by the gate;
    - region and frequency shown;
    - restore SF9 / 10 dBm afterwards;
  - `ad tcas off` → `lora:0`.

  Record SC-003 and SC-008 in `validation.md`.

**Checkpoint:** legal, budgeted beaconing works on the device.

---

## Phase 4: User Story 1 — See nearby watches on a radar (P1)

**Goal:** a TCAS page with a sweeping PPI, range rings and bogeys for every received contact.

**Independent test:** Q3 with simulated contacts: three contacts appear on rings 0/2/3 at stable bearings within
1 s, the sweep rotates, and contacts age out.

- [X] T020 [US1] Receive path in `TcasService` (research R5):
  - on each RX queue item:
    - `BeaconCodec::decode` OK → `table.update(..., simulated=false)`, `rx++`;
    - a decode failure → `foreign++`;
    - a CRC error → `crc++`;
    - **ignore our own `nodeId`**;
  - `table.age(now, tcasage)` once per second, logging `lost <call>`;
  - `new contact` log.

  Test hooks:
  - `ad tcas sim <id-hex> <callsign> <rssi>` (builds a `Beacon`, battery 50, SNR 8 dB, `simulated=true`, through
    the same update path);
  - `ad tcas simclear`.
- [X] T021 [US1] Add `PageId::Tcas` after Chrono in `src/ui/Page.h` and register it in `src/app/main.cpp` (page
  ring).
- [X] T022 [US1] Create `src/pages/TcasPage.{h,cpp}` (research R7):
  - **Layout:**
    - a header: own callsign, region/frequency, contact count, beacons sent, an ON/OFF toggle button (posts
      `TcasService::setEnabled`);
    - a square PPI below it with 4 range rings, a centre ownship symbol, and a sweep line with a fading trail
      (custom draw event, about 6 rotations/min, redrawn at 20 Hz);
    - each contact a small bogey object at (`bearingDeg`, ring radius), with clickable `ext_click_area` ≥ 48 px;
  - **States:**
    - TCAS off → a dim scope + "TCAS OFF";
    - `radioOk` false → "RADIO FAIL" (Ultra) / "RADIO N/A" (S3);
    - the 20 Hz timer runs only while the page is visible (`onShow` / `onHide`), and contacts refresh from
      `TcasSnapshot` at 2 Hz;
  - **NVG:** use the S7 `NvgStyle` colours (green tones only).
- [X] T023 [US1] **On-device Q3 (radar part) + Q6:**
  - the three simulated contacts on the right rings within 1 s;
  - bearings stable after re-injection;
  - the sweep continuous (**wearer**) and the PPI redraw ≥ 20 Hz while visible (`lv_timer` period log);
  - age-out after 60 s with `ad set tcasage 60` (then restore 300);
  - PFD fps median ≥ 30 with TCAS on;
  - the radar readable in NVG (**wearer**);
  - heap ≥ 40 KB.

  Record in `validation.md`.

**Checkpoint:** the radar shows contacts (simulated on one device).

---

## Phase 5: User Story 2 — Inspect a contact (P1)

**Goal:** tapping a bogey shows its details.

**Independent test:** Q3 card part: tap → callsign, RSSI, SNR, counting age, battery; LOST after age-out.

- [X] T024 [US2] In `src/pages/TcasPage.cpp` add the **detail card**:
  - a bogey click selects its `nodeId`;
  - an overlay card with callsign, the device-id suffix (last 4 hex digits of `nodeId`, so two equal callsigns are
    told apart), RSSI (dBm), SNR (dB), "heard N s ago" (updated at 2 Hz), battery %, ring, and `SIM` when simulated;
  - when the contact disappears, the card shows "LOST" for 3 s, then closes;
  - tapping the card or empty scope closes it;
  - NVG colours.
- [X] T025 [US2] **On-device Q3 (card part):**
  - tap each simulated bogey → the correct details, and the age counts up;
  - re-inject ALPHA at −90 three times → it moves outward (EMA);
  - age-out → LOST.

  Wearer: the bogey is easy to hit and the card is readable (verbatim). Record in `validation.md`.

**Checkpoint:** US1 + US2 + US3 complete.

---

## Phase 6: User Story 4 — TRAFFIC advisory (P2)

**Goal:** one TRAFFIC advisory (with the "Traffic" callout) per entry into the inner ring.

**Independent test:** Q4: −95 → −40 ×3 gives exactly one TRAFFIC; staying gives no repeat; out and back in gives
one more.

- [X] T026 [US4] In `TcasService`:
  - when `table.update` reports `enteredInner` → raise `AlertId::Traffic` through the existing alert path (the S5
    quiet mode and quiet hours apply) and log `[tcas] TRAFFIC <call>`;
  - clear the advisory when no contact is in the inner ring;
  - the wearer can acknowledge it through the existing AlertManager acknowledge path (like other advisories);
  - a TRAFFIC with the screen off wakes it through the extended alert-wake rule (T015: a speaking advisory wakes).
- [X] T027 [US4] **On-device Q4:**
  - −95 then −40 ×3 → exactly one TRAFFIC with the voice;
  - staying at −40 → no repeat;
  - out to −80 then back to −40 → one more;
  - 3 of 3 trials;
  - quiet mode silences the voice but keeps the annunciator;
  - with the screen off, a TRAFFIC turns it on;
  - acknowledging clears it like other advisories.

  Wearer: the voice is heard (verbatim). Record SC-005 (single-device part) in `validation.md`.

**Checkpoint:** all stories are complete on one device.

---

## Phase 7: Polish & Cross-Cutting

- [X] T028 **On-device Q5 (FR-014, SC-004, SC-007):**
  - USB meter, screen off: TCAS off (expected 34 mA) vs TCAS on; record both (the wearer reads the meter);
  - SC-004: TCAS off current = the S8 baseline;
  - **NFR-PWR-002:** the total screen-off current with TCAS on must stay ≤ 50 mA; if not, document it in the power
    report as an exception and tell the wearer;
  - duty persistence: a few beacons, eject, wake → `ad tcas` `duty` still shows the used time;
  - eject with TCAS on → wake → `ad tcas` on=1 and beacons continue.

  Add an S9 section to `docs/power_report.md` (TCAS-on drain, battery-life effect at 1100 mAh, TCAS off by
  default).
- [X] T029 [P] Create `docs/adr/015-lora-eu868-on-915-hardware.md` (research R1, R4):
  - the context (915 MHz unit, Türkiye);
  - the decision (EU868 g3 869.525 MHz, ≤ 14 dBm, 10 dBm default, 1 %/h self-imposed, US915 only where legal);
  - the consequences (reduced range, RSSI rings read farther).
- [X] T030 [P] Update `docs/software_requirements_specification.md` (plan "SRS amendments"):
  - OI-6 closed;
  - FR-TCS-001, 006 and 008 notes;
  - NFR-REG-001 note.
- [X] T031 [P] Update `docs/software_architecture_specification.md` (plan "SAS notes"):
  - §4 radio API;
  - §6 the TcasService row + the `radio` task;
  - §7.1 settings v10;
  - §7.2 18 B without position;
  - §8 the TCAS page in the ring;
  - the ADR log entry for ADR-015.
- [X] T032 [P] Update `README.md`: the TCAS page, the page ring, the `ad tcas` commands, the EU868 note, the S9
  contract link.
- [X] T033 Final checks:
  - `pio test -e native` all green;
  - both builds;
  - the flash/RAM delta vs T001.

  Complete `validation.md`:
  - SC-001…SC-008 with evidence;
  - SC-001, SC-002 and the real-device part of SC-005 marked **pending manual (second device)**.

  Mark all tasks [X].
- [X] T034 At merge, update `docs/product_roadmap.md` per the status tracking rule:
  - S9 → ✅ with the date, the PR number and the pending two-device checks;
  - the risks table: "915 MHz hardware in an 868 region: reduced range".

---

## Dependencies & Execution Order

- **T001** → **Foundational T002–T015**:
  - T002 → T003 → T004 (the radio probe gates everything that transmits);
  - T005–T010 (tests) before T011–T015;
  - T011 before T013 (`Beacon` type).
- **US3 (T016–T019)** needs T004, T011, T012, T014 and T015. T016 → T017 → T018 → T019.
- **US1 (T020–T023)** needs US3's service (T016) and T013. T020 ∥ T021; T022 needs T021; T023 last.
- **US2 (T024–T025)** needs T022.
- **US4 (T026–T027)** needs T020 and T015.
- **Polish (T028–T034)** after the stories. T034 at merge.

## Parallel Opportunities

- T005 ∥ T006 ∥ T007 ∥ T008 ∥ T009 ∥ T010 (tests; different directories).
- T012 ∥ T013 ∥ T014 after T011.
- T020 (service RX path) ∥ T021 (page id).
- Docs T029 ∥ T030 ∥ T031 ∥ T032.

### Parallel Example: Foundational

```text
T005 beacon_codec ∥ T006 lora_airtime ∥ T007 duty_cycle ∥ T008 tcas_table ∥ T009 lora_region ∥ T010 settings/alerts
T011 BeaconCodec + LoraAirtime → T012 DutyCycle ∥ T013 TcasTable ∥ T014 LoraRegion ; T015 Alerts + SettingDefs
```

## Implementation Strategy

- **The radio probe (T004) runs first:** if EU868 transmit on the 915 MHz hardware fails, stop and revise before
  building on it.
- **MVP = Phases 1–3 (US3):** legal, budgeted beaconing with the rail only while on. This is the only story fully
  verifiable with one device without simulation.
- Then US1 (radar) and US2 (card) with simulated contacts, then US4 (TRAFFIC).
- Stop and validate at each checkpoint; ask for wearer checks and record them verbatim.
- The two-device checks stay pending until a second AeroDeck watch or LoRa node is available.
