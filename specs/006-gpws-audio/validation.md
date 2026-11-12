# Validation: GPWS Audio (S5)

Device: T-Watch Ultra on COM3. Firmware built from branch `006-gpws-audio`. Scripts (session scratchpad):
`validate_s5.py`, `audio_loop.py`, `volume_steps.py`.

## T002 · Baseline (2026-11-11)
- `pio test -e native`: 161/161 (the 4 new empty S5 test folders errored as expected).
- `pio run -e twatch_ultra` ✅ (RAM 19.0 %, flash 47.4 %), `pio run -e twatchs3` ✅.

## T003 · Asset upload path (research R2)
- `pio run -e twatch_ultra -t buildfs` built `fatfs.bin` from `data/`; `-t uploadfs` wrote 8 253 440 bytes to
  0x811000 (hash verified).
- Read-back was checked with the first AudioService firmware instead of a temporary probe command (saves one
  build/flash): `ad audio list` → all 8 clips `ok` with valid headers (T016). **`uploadfs` works; no fallback needed.**
- Noted: LilyGoLib formats `ffat` if the mount fails (USB_MSC.cpp), so a broken image shows up as `MISSING`, not as a
  crash.

## T004 · Clips (`tools/gen_audio.py`)
Voice **Microsoft Zira Desktop** (System.Speech, offline). 16 kHz mono PCM16, trimmed at −45 dBFS, normalised
(−1 dBFS callouts, −3 dBFS chime), 10 ms fades. Total ≈ 293 KB.

| clip | duration |
|---|---|
| master_caution | 0.94 s |
| master_warning | 0.85 s |
| stall | 1.62 s |
| pull_up | 1.52 s |
| bingo_fuel | 0.73 s |
| minimums | 0.64 s |
| traffic | 1.80 s |
| chime (synth 880/660 Hz) | 1.05 s |

## T005–T013 · Host tests
`pio test -e native`: **184/184** (161 → 184: test_wav 4, test_audio_math 3, test_sound_queue 7,
test_audio_policy 6, test_alerts +2, test_settings +1).

## T014 · Board audio
- `IBoard::audioBegin/audioWrite/audioEnd` on both boards via `instance.player` (`configureTX(rate, 16, MONO)`,
  `i2s_zero_dma_buffer`). `audioReady_` = LilyGoLib ran `initAmplifier()` (no `NO_HW_MIC`).
- The analysis finding U1 assumed a bug in the Speaker ↔ Mic simulated-failure bit. On inspection it is deliberate:
  LilyGoLib's single `NO_HW_MIC` flag skips both the PDM mic and the amplifier I2S init. No board change; the test
  uses `ad simfail mic`.

## T016 · Device checkpoint
- `ad audio list`: 8 × `ok`, 16000 Hz, durations match the files.
- Timing (quiet hours temporarily off with `qhstart = qhend = 0`): `amp ON` → `play` 127 ms (30 ms settle + 20 ms
  zeros + FFat open/parse), stream time ≈ clip duration, the 4096-sample flush takes ≈256 ms (= 16 kHz mono: correct
  speed, the `ALL_RIGHT` + `I2S_CHANNEL_MONO` configuration needs no L/R duplication), **`amp OFF after` 405–406 ms**.
- **Quiet hours worked live:** at 22:24 local the Sample request was `suppressed (quiet hours)`.
- **Wearer, first listen (volume 2 = 0.35):** "too quiet". At volume 4: "Max is fine".
  → Volume curve raised to {Off, 0.30, 0.50, 0.71, 1.0} (≈ −10.5 / −6 / −3 / 0 dB).
- **Wearer, volume steps 1–4/Off with the new curve:** "Step 2 still quiet" → **default volume changed 2 → 3**
  (spec FR-011, data-model §6, research R7, SettingDefs, test).
- Tuned values: settle 30 ms, guard zeros 20 ms, DMA flush 4096 samples, tail 150 ms.

## T018 · US1 callouts (SC-001) — `validate_s5.py callouts repeat misc preempt`
| check | result |
|---|---|
| STALL ×10 → `stall` | latency raise→play min 134 / median 135 / max 136 ms ✅ |
| PULL UP ×10 → `pull_up` | 135 / 135 / 135 ms ✅ |
| CLOCK NOT SET (CAUTION) ×10 → `master_caution` | 135 / 135 / 135 ms ✅ |
| warning repeat | plays at +0, +5999, +11999, +17999 ms (intervals 6.0 s) ✅ |
| ack during a repeat | `[aud] stop (ack)` in the same ms as `[alert] ack`; clip cut mid-play; 0 plays in the next 10 s ✅ |
| `test2 C` → minimums · `test1 A` → none · `timeunsync` → none · `test1 W` → minimums | ✅ |
| preempt: chime, then STALL 300 ms later | stall started 390 ms after the chime (chime is 1050 ms) ✅ |

The first run reported 3 failures that were **lost serial log lines** (USB CDC drops bytes; known since S0): every
callout had played (`amp ON` + `play` present, or the `played` counter incremented). The script now cross-checks the
counter; the re-run had 0 drops and passed.

## T019 · US2 amplifier (SC-002)
- 20 plays (mixed, incl. back-to-back): `amp OFF after` min 256 / median 406 / max 406 ms ✅; idle `amp=OFF` ✅;
  `ad set volume 0` → `OK suppressed (volume off)`, amp stays OFF ✅.
- Speaker failure (`ad simfail mic` + reboot): SPEAKER `SIMFAIL`; STALL raised → stick shaker every 2 s, callout
  counted as skipped, no `amp ON`, no hang; ack works ✅. Restored (`simfail none`).
- **60-minute idle log** (`validate_s5.py idle:60`, 2026-11-12 09:2x–10:2x local): exactly one `amp ON`, belonging to
  the scheduled **10:00 chime** (`chime hh=10` → `play chime` → `amp OFF after 405 ms`); no unexplained `amp ON` ✅.
  The script first reported FAIL because the audio task logged `amp ON` ~1 ms before the svc task printed its
  `chime hh=10` line; the correlation window was fixed to ±1 s. Bonus: a real hourly chime observed live.
- **Wearer, pops:** "All good" for all 8 clips (no pops/clicks at start or end) ✅.

## T018 · Wearer listening (STALL + ack)
- "Works": "Stall. Stall." with the shaker, repeating; tap on the band stopped it.
- Log re-check (a status read taken before the wearer's tap had shown `acked=0`): plays every 6.0 s (5 plays), tap →
  `[alert] ack 1`, STALL `acked=1`, no plays afterwards ✅. (The tap fell between two repeats, so there was nothing
  to cut; mid-clip cutting is shown by the scripted ack test above.)

## T020/T021 · US3 hourly chime (SC-003) — `chime_live.py` (tz UTC+3, quiet hours 22–7)
| case | result |
|---|---|
| 5 hours outside quiet hours (local 10, 11, 12, 13, 14:00; clock set to hh:59:50) | 5/5 chime + play, at +10.2…10.9 s after setting hh:59:50 (≤0.9 s after hh:00) ✅ |
| quiet hours 23:00, 00:00, 03:00 | 0/3 ✅ |
| quiet mode (15:00), chime off (16:00) | 0/2 ✅ |
| chime at 17:00, then the clock set back to 17:00:02 | no second chime ✅ |
| clock restored with `ad sync` | ✅ |

`chimecheck` table: with 23–6 after a reboot: 22:00:00 → due=1, 23:00:00 → quiet-hours, 06:00:01 → due=1; with the
chime off → `disabled` ✅ (the host tests cover the full table, incl. `not-hh00`, `quiet`, `volume`, `already`).

## T022/T023 · US4 settings (SC-004 volume part)
- Settings → SOUND: Volume Off/1–4, Hourly chime, Quiet from / Quiet until (00:00…23:00), Play sample.
- **Wearer:** "All good": each step louder, Off silent, section readable, dropdowns work.
- Persistence: volume 4, chime 0, qh 23–6 set → reboot → `ad audio` shows `volume=4 chime=0 qh=23-6` ✅; then
  restored to 3 / 1 / 22–7.

## T024 · US5 clip library (SC-004, SC-006)
- `ad audio list`: 8 × ok (see T016). **Wearer:** "All good": all 8 clear, complete, similar loudness, no pops.
- `traffic.wav` removed + `uploadfs`: `traffic MISSING`; two plays → one `[aud] skip traffic: missing` line,
  `skipped=2`, amp stayed OFF, watch normal ✅.
- **No clips at all** (empty `data/audio/` + `uploadfs`): normal boot (sync + weather OK), all 8 `MISSING`; STALL →
  `skip stall: missing` once, stick shaker plays and stops on ack (`[hap] play shaker` / `stop shaker (ack)` at log
  level debug), no hang ✅. Clips restored and re-uploaded (8 × ok).
- Reproducibility: re-running `tools/gen_audio.py` gave **byte-identical** WAVs (SHA-256 of all 8 equal).

## T028 · Regression and performance
- `pio test -e native` 184/184; `pio run -e twatch_ultra` ✅ (RAM 19.5 %, flash 48.0 %); `pio run -e twatchs3` ✅.
- Layering grep: no `i2s_` / `instance.player` / `driver/i2s` outside `src/board/`; `FFat` only in
  `src/services/AudioService.cpp`; no `lv_` in services/core; no LilyGoLib in services/pages/ui/core ✅.
- S0/S1 regression (`validate_s1.py regress`): page ring and button navigation, all peripherals OK ✅.
- S2/S3: every boot in this session synced time (`sync: Ok`) and fetched the METAR (`fetch: Ok stn=LTAC`) ✅.
- S4 (`validate_s4.py order ack`): ordering 10/10 (a first run showed 9/10 from a dropped serial line; re-run 10/10),
  ack stops the shaker, a new caution after ack plays ✅.
- **SC-005** (`audio_perf.py`, PFD + `att sweep on`): baseline median 29 fps (min 28); during 20 s of continuous
  clips median **29** fps (min 27, n=34), heap min **50 988 B** (≥40 KB) ✅.
