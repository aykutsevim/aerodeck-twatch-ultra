# Quickstart & Validation: GPWS Audio (S5)

**Prerequisites:** as in S4. Listening checks need the wearer.

## Q1. Build, tests, assets
```powershell
python tools/gen_audio.py                 # regenerate data/audio/*.wav (optional; files are committed)
pio test -e native                        # + test_wav, test_audio_math, test_sound_queue, test_audio_policy, settings v6
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatch_ultra -t uploadfs --upload-port COM3     # writes data/ to the ffat partition (ADR-013)
pio run -e twatchs3
```
`ad audio list` → all 8 clips `ok`, with durations.

## Q2. Callouts (US1, SC-001)
- `ad alert raise stall` → "Stall. Stall." within 1 s, together with the shaker; repeats about every 6 s.
- `ad alert ack` (or a tap) → speech stops ≤0.5 s, with no further repeats.
- `ad alert raise pullup` → "Pull up".
- `ad alert raise clocknotset` (a CAUTION with no clip of its own) → "Master caution" once.
- `ad alert raise test2 C` → "Minimums" once. `ad alert raise test1 A` → no voice (Advisory, FR-005).
- `ad wifi age 90000` → TIME UNSYNC (advisory), no voice.
- Preempt: `ad audio play chime`, then immediately `ad alert raise stall` → the chime is cut and "Stall" plays.
- Script ×10 for each (latency from the `[alert] raise` log to `[aud] play`).

## Q3. Amplifier (US2, SC-002)
- `ad audio` idle → `amp=OFF`.
- 20 plays: every `[aud] amp OFF after <ms>` is ≤500 ms after the play ends.
- A **60-minute** idle log has no `amp ON` without a play (SC-002).
- Speaker failure: `ad simfail mic` + reboot (the speaker shares the mic bit: LilyGoLib's `NO_HW_MIC` also skips the amplifier) → `ad alert raise stall` → the shaker
  vibrates, `[aud] output unavailable` once, no sound, no hang. Clear it afterwards.
- The wearer listens for pops at start and stop.

## Q4. Chime rule (US3, SC-003)
- `ad audio chimecheck 14:00:02` → due=1; `14:00:06` → not-hh00; `23:00:01` → quiet-hours; `06:59:59` → not-hh00;
  `07:00:00` → due=1.
- With `ad set quiet 1` → quiet; with `ad set chime 0` → disabled.
- Live: set the clock to hh:59:50 outside quiet hours (`ad time set …` in UTC) → one chime at hh:00 (log
  `[aud] chime`), 5 hours.
- 5 negative cases → none: 3 hours inside quiet hours, one with `ad set quiet 1`, one with `ad set chime 0`
  (SC-003).
- A clock change into hh:00:02 of an hour that already chimed → no second chime.

## Q5. Volume and settings (US4, SC-004)
- Settings → SOUND → Volume Off/1–4 + Play sample: the wearer confirms each step is louder.
- Volume Off → nothing plays and `amp=OFF`.
- Quiet hours 23–6 → the chimecheck results follow. Reboot keeps the settings.

## Q6. Library and robustness (US5, SC-004, SC-006)
- `ad audio play <each>`: the wearer judges clarity and loudness.
- Rename one file? FFat is not writable over USB. Instead, flash an image without `traffic.wav` (temporarily remove
  it from `data/`, `uploadfs`) → `ad audio list` shows `traffic MISSING`; `ad audio play traffic` → `skip` logged; the
  watch keeps working. Restore it.
- **No files at all (SC-006):** flash an image with an empty `data/audio/` → the watch boots normally, `ad audio
  list` shows all 8 `MISSING`, `ad alert raise stall` still vibrates, one skip line per clip, no hang. Restore and
  re-upload.

## Q7. Performance (SC-005)
- `ad att sweep on`, a loop of `ad audio play minimums` for 20 s: `ad stat` FPS median ≥28, heap ≥40 KB. The wearer
  sees no stutter.
