---

description: "Task list for GPWS Audio, Voice Callouts & Cabin Chime (Slice S5)"
---

# Tasks: GPWS Audio, Voice Callouts & Cabin Chime (Slice S5)

**Input**: Design documents from `/specs/006-gpws-audio/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V and spec FR-016 require Unity tests for the new `src/core/` modules,
written before the implementation.

**Organization**:
- US1: spoken warnings (P1)
- US2: amplifier only while playing (P1)
- US3: hourly chime (P2)
- US4: volume and sound settings (P2)
- US5: clip library (P3)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. Native tests
  need WinLibs GCC on PATH. Serial tools open COM3 with **DTR/RTS low**.
- **Rules from S0–S4 still hold:**
  - I2S/`Player` only in `src/board/`;
  - `src/core/` is pure;
  - no LVGL in services;
  - the amplifier rail only via `PowerService` `RailLease(Rail::AudioAmp)`;
  - no audio work on the UI task.
- **Write helper scripts with the Write tool** (shell heredocs mangle backslashes on this machine).
- **Listening checks need the wearer.** Ask, and record the answer verbatim in `validation.md`.
- **References:** contracts in `contracts/serial_commands.md`; entities in `data-model.md`; decisions in
  `research.md` R1–R8.

---

## Phase 1: Setup

- [X] T001 Create the test folders `test/test_wav/`, `test/test_audio_math/`, `test/test_sound_queue/` and
  `test/test_audio_policy/`, and the asset folder `data/audio/`
- [X] T002 Verify the baseline on branch `006-gpws-audio`: `pio test -e native` (161/161), `pio run -e twatch_ultra`
  and `pio run -e twatchs3`. Create `specs/006-gpws-audio/validation.md`.
- [X] T003 **Verify the asset upload path** (research R2):
  1. Put `data/audio/probe.txt` in place and run `pio run -e twatch_ultra -t uploadfs --upload-port COM3`.
  2. Reboot, and check that the LilyGoLib boot listing (or a temporary `ad audio list`-style check) shows
     `/audio/probe.txt`.
  3. If `uploadfs` for fatfs fails, implement the fallback instead: `tools/upload_audio.py` + a serial `ad fs put`.

  Record the result in `validation.md`, then remove `probe.txt`.

---

## Phase 2: Foundational (blocking)

**Purpose:** the clip assets, the pure audio core, settings v6, IBoard audio, and the AudioService engine.

### Assets

- [X] T004 `tools/gen_audio.py` (research R3):
  - for each phrase, run PowerShell `System.Speech` with voice **Microsoft Zira Desktop** (fallback Hazel) →
    `SetOutputToWaveFile(tmp, SpeechAudioFormatInfo(16000, Sixteen, Mono))`;
  - post-process with stdlib `wave`/`array`: trim silence (threshold −45 dBFS), peak-normalise to −1 dBFS, 10 ms
    fades;
  - write `data/audio/{master_caution,master_warning,stall,pull_up,bingo_fuel,minimums,traffic}.wav`;
  - synthesize `chime.wav`: 880 Hz then 660 Hz, 350 ms each, exponential decay, peak −3 dBFS;
  - print the voice used and each duration;
  - write `data/audio/README.md` (engine, voice, phrases, date, "original TTS, no third-party audio").

  Run it and commit the WAVs.

### Tests (write first)

- [X] T005 [P] Write `test/test_wav/test_main.cpp` for `core::Wav::parse` (data-model §2):
  - a minimal valid PCM16 mono 16 kHz header;
  - a header with a LIST chunk before `data`;
  - rejects: stereo, 8-bit, IEEE float (format 3), rate 4000/96000, missing `data`, a truncated buffer, `RIFF` with a
    wrong `WAVE` tag, a data size beyond the file size;
  - the real generated `data/audio/chime.wav` header bytes (the first 44 bytes embedded as an array) parse to the
    expected rate and length.
- [X] T006 [P] Write `test/test_audio_math/test_main.cpp` for `core::AudioMath` (data-model §3):
  - `gainQ15` for steps 0..4 = {0, 9830, 16384, 23170, 32767} (raised at T016); out-of-range step → 0;
  - `applyGain` scales and saturates (32767 × full stays 32767; −32768 stays in range); gain 0 → silence;
  - monotonic loudness across steps.
- [X] T007 [P] Write `test/test_sound_queue/test_main.cpp` for `core::SoundQueue` (data-model §4):
  - pop order by priority, FIFO within a priority;
  - the same clip at the same priority → `Duplicate`;
  - full (6) → a higher priority evicts the lowest; an equal or lower priority → `Full`;
  - `pop` drops entries older than 5000 ms, and a Chime older than 2000 ms (`maxAgeMs`), incl. `millis` wrap;
  - `purge(Caution)` removes Caution and Warning but keeps Chime/Sample;
  - `shouldPreempt`.
- [X] T008 [P] Write `test/test_audio_policy/test_main.cpp` for `core::AudioPolicy` (data-model §5):
  - `inQuietHours`: 22–7 at hours 21, 22, 23, 0, 6, 7; 23–6; 0–0 (none); 7–22 (daytime window);
  - the `allow` matrix: quiet / volume 0 / quiet hours × priority (only Warning passes in quiet hours);
  - `chimeDue`:
    - 14:00:00, 14:00:04 → due; 14:00:05 → no;
    - a second call in the same hour → no (`already`);
    - 23:00:00 → no (quiet hours);
    - 07:00:00 → due;
    - clock invalid, disabled, quiet, volume 0 → no;
    - a clock jump to 15:00:02 after chiming at 15:00:00 → no;
    - a jump from 13:59 to 16:20 → no catch-up;
    - a reason string for each "no".
- [X] T009 [P] Extend `test/test_alerts/test_main.cpp`:
  - `calloutFor` (Advisory checked first, data-model §1): Stall → Stall, PullUp → PullUp, Test1 at Caution →
    Minimums, Test2 at Warning → Minimums, **Test1 at Advisory → None**, ClockNotSet → MasterCaution, TimeUnsync →
    None, a peripheral id → MasterCaution, an unknown Warning → MasterWarning;
  - `clipFile`/`clipFromName` round-trip for all 8 clips; an unknown name → false.
- [X] T010 [P] Extend `test/test_settings/test_main.cpp` for schema v6:
  - `kSchemaVersion == 6`;
  - `volume` U8 0..4 default 3 (changed at T016); `chime` Bool default 1; `qhstart` U8 0..23 default 22; `qhend` U8 0..23 default 7;
    all editable;
  - migration 5→6 is additive.

### Implementation

- [X] T011 Implement `src/core/Wav.h/.cpp`, `src/core/AudioMath.h/.cpp`, `src/core/SoundQueue.h/.cpp` and
  `src/core/AudioPolicy.h/.cpp` so T005–T008 pass
- [X] T012 Extend `src/core/Alerts.h/.cpp`:
  - the `Clip` enum, `clipFile`, `clipFromName`, `calloutFor`;
  - fill `voiceClip` in the table (Stall, PullUp, Test1..3 → Minimums).

  Make T009 pass.
- [X] T013 Settings v6 in `src/core/SettingDefs.h/.cpp` (`Volume`, `Chime`, `QhStart`, `QhEnd`, schema 6) so T010
  passes
- [X] T014 IBoard audio (research R1):
  - `src/board/IBoard.h`: `bool audioBegin(uint32_t rateHz)`, `size_t audioWrite(const int16_t* s, size_t n)`,
    `void audioEnd()`;
  - implement in `src/board/BoardUltra.cpp` and `src/board/BoardS3.cpp` via `instance.player`:
    `configureTX(rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO)` + `i2s_zero_dma_buffer`; `write`; zero buffers
    on end;
  - return false/0 unless `periph(Periph::Speaker) == Ok`.

  The Speaker periph state already exists from S0. It deliberately shares the **Mic** simulated-failure bit:
  LilyGoLib's single `NO_HW_MIC` flag skips both the PDM mic and `initAmplifier()` (no I2S driver). So
  `ad simfail mic` + reboot simulates a missing speaker; no board change is needed (verified during implement).
- [X] T015 `src/services/AudioService.h/.cpp` (research R4, data-model §7):
  - `begin()` creates the `audio` task (core 0, prio 5, 4 KB stack) and a request queue;
  - `play(Clip, SoundPrio)` returns `PlayResult` (`Queued/Suppressed/Duplicate/Full/NoClip`, data-model §7);
    `Suppressed` comes from `AudioPolicy::allow` with the current quiet/volume/quiet-hours state, and carries the
    reason for the console reply;
  - `stop(minPrio)`, `stopAlerts()`, `status()`;
  - **the task:**
    - `SoundQueue::pop`;
    - before the first clip, `RailLease(AudioAmp)` + 30 ms settle + 20 ms zeros;
    - open `FFat` `/audio/<file>`, read 256 header bytes, `Wav::parse`;
    - stream 512-sample chunks with `applyGain(gainQ15(volume))` to `board().audioWrite`;
    - check the stop/preempt flags per chunk;
    - 20 ms zeros after each clip;
    - when the queue is empty, a 200 ms tail, then release the lease (log `[aud] amp OFF after <ms>`);
  - skip a missing or bad clip with one log line per clip per boot;
  - **output unavailable** (speaker not `Ok` or `audioBegin` false): drop the queue (`skipped += n`), release the
    lease, log `[aud] output unavailable` once per boot, and wait for the next request (no retry loop);
  - serial `ad audio`, `ad audio play`, `ad audio stop`, `ad audio list` (contracts/serial_commands.md).

  Call `begin()` in `src/app/main.cpp` after `PowerService::begin`.
- [X] T016 **Checkpoint on the device:**
  - flash the firmware and `uploadfs`;
  - `ad audio list` → 8 clips ok;
  - `ad audio play minimums` → **ask the user**: heard clearly, correct speed (not chipmunk/slow), no pop at start or
    end;
  - compare the `ms=` duration with the file duration;
  - `ad audio` → `amp=OFF` afterwards, and `[aud] amp OFF after` ≤500 ms.

  If the speed is wrong (stereo slot), duplicate samples into L/R (plan note 3). Tune the settle/tail values. Record
  everything in `validation.md`.

**Checkpoint:** `pio test -e native` all green, both firmware targets build, and a clip is heard.

---

## Phase 3: User Story 1 - The watch speaks its warnings (P1) 🎯 MVP

**Goal:** callouts on raise/re-arm, warning repeats every ~6 s, stop on acknowledge, preemption.

**Independent Test:** quickstart Q2.

- [X] T017 [US1] AlertManager callouts in `src/services/AlertManager.cpp` (research R5):
  - on `Added`/`Rearmed` (not acknowledged) → `AudioService::play(calloutFor(id, level), Warning|Caution prio)`;
    Advisory/None → no call;
  - in `tick`, every 3rd stick-shaker repeat (≈6 s) re-queue the warning callout of the master unacknowledged
    WARNING. Reset the counter whenever the shaker is (re)started by a raise or re-arm, so the first repeat comes
    at ≈6 s, not 2 s;
  - `ackAll()` → `AudioService::stopAlerts()`.
- [X] T018 [US1] Validate quickstart Q2 with a script (`validate_s5.py`, Write tool):
  - 10 raises each of stall / pullup / **clocknotset** (CAUTION → `master_caution`) → the latency from the
    `[alert] raise` log to `[aud] play <clip>` is ≤1 s; the clip is correct;
  - `test2 C` → `minimums` once; `test1 A` → no play (Advisory);
  - warning repeat intervals ≈6 s;
  - `ad alert ack` → `[aud] stop` within 0.5 s, then no repeats for 10 s;
  - TIME UNSYNC → no play;
  - preempt: chime then stall → the chime stops and stall plays.

  **The user listens** to one STALL + ack. Record SC-001 in `validation.md`.

---

## Phase 4: User Story 2 - Silent when nothing plays (P1)

**Goal:** the amplifier is powered only while sounds play; no pops.

**Independent Test:** quickstart Q3.

- [X] T019 [US2] Validate quickstart Q3:
  - 20 plays (mixed clips, back-to-back and spaced): every `amp OFF after` ≤500 ms; `ad audio` idle shows `amp=OFF`;
  - a **60-minute** idle log (run in the background while T020–T022 are written) has no `amp ON` without a
    `play` (SC-002);
  - `ad simfail mic` + reboot (speaker shares the bit, T014) → `ad alert raise stall` → shaker vibrates, `[aud] output unavailable` once, no hang;
    clear the simulated failure afterwards;
  - `ad set volume 0` → `ad audio play chime` → `suppressed`, amp stays OFF;
  - **the user** confirms no pops.

  Record SC-002 in `validation.md`.

---

## Phase 5: User Story 3 - Hourly cabin chime (P2)

**Goal:** a chime at hh:00 outside quiet hours; never duplicated after clock changes.

**Independent Test:** quickstart Q4.

- [X] T020 [US3] Chime scheduler in `src/services/AudioService.cpp`:
  - `tick(nowMs)` on the svc task (1 Hz): local seconds from `ClockSnapshot` (`utc + tzOffsetMin*60`) →
    `AudioPolicy::chimeDue(...)` → `play(Clip::Chime, SoundPrio::Chime)`, remember `hourKey`;
  - log `[aud] chime hh=<h>`;
  - serial `ad audio chimecheck <HH:MM:SS>` (today's local date + the real `lastHourKey`, so `already` only for an
    hour that actually chimed) and `ad audio chimenow` (contracts).

  Register the tick in `src/app/main.cpp`.
- [X] T021 [US3] Validate quickstart Q4:
  - the chimecheck table (all cases in the contract);
  - live: `ad time set` to hh:59:50 UTC (pick an hour whose local hour is outside quiet hours) → one `[aud] chime`
    at hh:00 (±2 s), 5 hours by stepping the clock;
  - **5 negative cases** → none: 3 hours inside quiet hours, one with quiet mode, one with the chime off;
  - a clock jump back into hh:00:02 → no second chime;
  - restore the clock with `ad sync` afterwards.

  Record SC-003 in `validation.md`.

---

## Phase 6: User Story 4 - Volume and sound settings (P2)

**Goal:** a SOUND section in Settings; settings persist.

**Independent Test:** quickstart Q5.

- [X] T022 [US4] Settings section "SOUND" in `src/pages/SettingsPage.cpp`:
  - "Volume" dropdown Off/1/2/3/4;
  - "Hourly chime" switch;
  - "Quiet hours" row with two dropdowns of `00:00`…`23:00` (start, end);
  - "Play sample" button → `AudioService::play(Chime, Sample)`.

  All with `HapticClick::attach` and `noSwipe` on interactive controls. Refresh on `SettingsChanged`.
- [X] T023 [US4] Validate quickstart Q5:
  - **the user** plays the sample at Off/1–4 and confirms each step is louder and Off is silent;
  - quiet hours changed → the chimecheck follows;
  - a reboot keeps the settings.

  Record SC-004 (volume part) in `validation.md`.

---

## Phase 7: User Story 5 - Clip library (P3)

**Goal:** all 8 clips present, clear, of similar loudness, reproducible; a missing clip is handled.

**Independent Test:** quickstart Q6.

- [X] T024 [US5] Validate quickstart Q6:
  - `ad audio list` → 8 ok with durations;
  - **the user** listens to each clip (`ad audio play <each>`) and confirms clarity and similar loudness;
  - a missing-clip test: remove `traffic.wav` from `data/`, `uploadfs` → `traffic MISSING`, play → `skip` logged,
    the watch is fine; restore it and re-upload;
  - **no files at all (SC-006):** `uploadfs` with an empty `data/audio/` → normal boot, `ad audio list` all
    `MISSING`, `ad alert raise stall` still vibrates, one skip line per clip, no hang; restore and re-upload;
  - re-run `tools/gen_audio.py` → the same files (byte-identical or with matching durations; record which).

  Record SC-004/SC-006 in `validation.md`.

---

## Phase 8: Polish & Cross-Cutting

- [X] T025 [P] Write `docs/adr/013-audio-assets-uploadfs.md`:
  - **context:** ADR-009 assumed USB MSC; `ARDUINO_USB_MODE=1` compiles it out;
  - **decision:** `data/audio` + `uploadfs`; FFat is read in services as a platform service (ADR-012 reasoning);
    I2S via IBoard;
  - **consequences:** the factory MP3s are replaced; the upload step is documented.

  Add the ADR-013 row to the SAS ADR table.
- [X] T026 [P] Amend `docs/software_requirements_specification.md` (FR-AUD-001/002/003/005 notes per plan) and
  `docs/software_architecture_specification.md`:
  - the AudioService row (task, queue, lease, chime, policy);
  - the IBoard audio methods;
  - §7.1 assets/upload;
  - the SettingsStore row (v6);
  - stale LittleFS/USB wording: SAS AudioService row ("WAV from LittleFS" → FFat), the SAS source-tree line
    "(assets) … copy over USB mass storage (ADR-009)" → `uploadfs` (ADR-013), SRS §2.3 and the filesystem bullet
    "LittleFS (or FAT)" / "LittleFS/FAT" → FAT `ffat`.
- [X] T027 [P] Update `README.md`:
  - sounds (callouts, chime, quiet hours, volume);
  - **the asset upload step** (`pio run -t uploadfs`, which replaces the factory MP3s);
  - `tools/gen_audio.py`;
  - the `ad audio …` commands, with a link to the S5 contract.
- [X] T028 Regression and performance:
  - `pio test -e native`; `pio run -e twatchs3`;
  - layering grep (`i2s_`/`instance.player` only in `src/board/`; `FFat` only in `src/services/AudioService.cpp`;
    no `lv_` in services);
  - the S1 regression script;
  - S2/S3 boot sync + weather;
  - S4 alerts/haptics still behave (`validate_s4.py order ack`);
  - **SC-005:** `ad att sweep on` + a 20 s callout loop → FPS median ≥28, heap ≥40 KB.

  Record in `validation.md`.
- [X] T029 After the PR is merged, update `docs/product_roadmap.md` per the **status tracking rule**: S5 → ✅ Done
  with the date, the PR number and the pending checks; M2 stays 🚧 until S6. Fix the S5 row text "WAV callouts
  from LittleFS" → "from the FAT partition".

---

## Dependencies & Execution Order

- **Setup (T001–T003)** → **Foundational (T004–T016)**:
  - T004 (assets) is needed by T005 (embedded header) and T016;
  - tests T005–T010 come before T011–T013;
  - T014 comes before T015, which comes before T016.
- **US1 (T017–T018)** needs Foundational.
- **US2 (T019)** needs T015 (it can run right after T016).
- **US3 (T020–T021)** needs T011 (AudioPolicy) and T015.
- **US4 (T022–T023)** needs T013 and T015.
- **US5 (T024)** needs T004 and T015.
- **Polish:** after the stories. T029 runs only after the merge.

### Parallel Opportunities

- T005 ∥ T006 ∥ T007 ∥ T008 ∥ T009 ∥ T010 (test files).
- T020 (AudioService chime) and T022 (SettingsPage) touch different files.
- T025 ∥ T026 ∥ T027.

## Parallel Example: tests first

```text
Task: "T005 WAV parser test in test/test_wav/test_main.cpp"
Task: "T006 gain test in test/test_audio_math/test_main.cpp"
Task: "T007 sound queue test in test/test_sound_queue/test_main.cpp"
Task: "T008 audio policy / chime rule test in test/test_audio_policy/test_main.cpp"
```

## Implementation Strategy

1. **MVP = Foundational + US1 + US2.** Spoken warnings, with the amplifier off when silent.
2. **US3 chime, then US4 settings.**
3. **US5 library checks.**
4. **Docs, regression, merge.** Then the roadmap update (T029).

## Notes

- **If an installed API differs** from the task text (e.g. `Player::configureTX` arguments, the FFat mount path
  `/fs`, the `uploadfs` target name for fatfs): follow the installed sources and note it in `validation.md`.
- **Never commit third-party audio.** Every WAV comes from `tools/gen_audio.py`.
