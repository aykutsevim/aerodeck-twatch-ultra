# Research: GPWS Audio, Voice Callouts & Cabin Chime (Slice S5)

Checked on 2026-11-11 against:
- LilyGoLib 0.2.0 (`LilyGoWatchUltra.cpp`, `PDM.h/.cpp`, `USB_MSC.cpp`);
- the board variants (`variants/lilygo_twatch_ultra|s3/pins_arduino.h`);
- `platformio.ini`, `partitions/aerodeck_16MB.csv`, ADR-009, and the S0–S4 sources.

## R1 · Audio output path
- **Finding:**
  - Both variants define `USING_PCM_AMPLIFIER` and the I2S pins (Ultra BCLK 9 / WCLK 10 / DOUT 11; S3 48 / 15 / 46).
  - Under IDF 4.4, LilyGoLib's `begin()` calls `initAmplifier()` → `Player::init(BCLK, WCLK, DOUT)`. This **installs
    the legacy I2S driver** on the player port (master TX, 16-bit, `I2S_CHANNEL_FMT_ALL_RIGHT`, 4 × 1024 DMA,
    `tx_desc_auto_clear`).
  - `Player` exposes `configureTX(rate, bits, ch)` (→ `i2s_set_clk`) and `write(buf, n)` (→ `i2s_write`).
  - The amplifier power is PMU BLDO2 (`POWER_SPEAK`), already mapped to `Rail::AudioAmp` in `BoardUltra::setRail`.
  - `ESP8266Audio` 2.0.0 is pinned in `platformio.ini`, but nothing in `src/` uses it.
- **Decision:** stream through LilyGoLib's `Player` behind three new IBoard methods (board layer only, constitution II):
  - `bool audioBegin(uint32_t rateHz)`: `configureTX(rate, 16, I2S_CHANNEL_MONO)` + `i2s_zero_dma_buffer`;
  - `size_t audioWrite(const int16_t* s, size_t n)`: blocking until DMA accepts; called only from the audio task;
  - `void audioEnd()`: zero the DMA buffers so no stale samples replay.

  WAV parsing and volume run in our own code: a pure `core::Wav` header parser and `core::AudioMath` q15 gain.
  `ESP8266Audio` is not used. It stays pinned because removing a pin is out of scope; it is noted in the plan.
- **Rationale:**
  - A second `AudioOutputI2S` would try to install a driver on an already-installed port (conflict).
  - Our clips are plain 16-bit PCM WAV, so no decoder library is needed (constitution VIII).
  - The parser is host-testable (V).
- **Alternatives:**
  - `ESP8266Audio` (`AudioFileSourceFS` + `AudioGeneratorWAV` + `AudioOutputI2S`) after `instance.player.end()`:
    two owners of one peripheral, more RAM. Rejected.

## R2 · Getting the clips onto the watch (ADR-009 follow-up)
- **Finding:**
  - LilyGoLib mounts FAT (`FFat.begin(false, "/fs")`) on the `ffat` partition (~7.9 MB) at boot.
  - Its USB mass-storage export is compiled only `#if !ARDUINO_USB_MODE`. Our board JSON sets
    `-DARDUINO_USB_MODE=1` (HW CDC/JTAG, needed for the serial console). So **USB drag-and-drop (an ADR-009
    consequence) is not available.**
- **Decision:** clips live in the repo under `data/audio/*.wav` and are flashed with **`pio run -t uploadfs`**.
  - `board_build.filesystem = fatfs` is already set, so the platform builds a FAT image of `data/` and writes it to
    `ffat`.
  - This replaces the LilyGoLib factory MP3s, which are not used.
  - It is recorded in **ADR-013**, which amends ADR-009's consequence.
- **Fallback** (if `uploadfs` for fatfs fails on this platform version): a serial upload helper
  (`ad fs put <name> <size>` + raw bytes) written in `tools/upload_audio.py`. The choice is recorded in
  `validation.md`.
- **Rationale:** reproducible from a clean checkout (constitution VII); no USB-mode change (which would break the
  serial console).

## R3 · Clip format and generation (FR-008, constitution VI)
- **Format:** WAV, PCM 16-bit, mono, **16 kHz**, peak-normalised to −1 dBFS, leading/trailing silence trimmed, 10 ms
  fade-in/out. Callouts are about 0.5–1.2 s, so 16–40 KB each; the total is ≲300 KB.
- **Voices:** Windows `System.Speech.Synthesis.SpeechSynthesizer` (built into Windows, offline), using the installed
  English voice (Microsoft Zira; verified 2026-11-11 that this PC has "Microsoft Zira Desktop" en-US and "Microsoft
  Hazel Desktop" en-GB. Zira is a female voice in the classic cockpit-voice style; Hazel is the fallback). Output goes straight to `SpeechAudioFormatInfo(16000,
  Sixteen, Mono)`.
  - `tools/gen_audio.py` drives a PowerShell snippet per phrase, then post-processes the result (trim, normalise,
    fade) with the Python stdlib (`wave`, `array`).
  - Phrases: "Master caution", "Master warning", "Stall. Stall.", "Pull up. Pull up.", "Bingo fuel", "Minimums",
    "Traffic. Traffic.".
- **Chime:** synthesized in `gen_audio.py`: two-tone "bing-bong" (880 Hz then 660 Hz, 350 ms each, exponential decay,
  soft). No recording is involved.
- **Reproducibility:** the clips are committed **and** regenerable. The script prints the voice name used, and
  `data/audio/README.md` records the engine, the voice, the phrases and the date.
- **Alternatives:** online TTS services (account and network needed, licence terms vary); recordings from aircraft or
  films (forbidden, constitution VI).

## R4 · Playback engine (`AudioService`, the `audio` task)
- **Task:** core 0, priority 5 (AD-SAS-001 §5.1), stack 4 KB. Its I2S writes block on DMA, so it never polls.
- **Queue:** pure `core::SoundQueue` (capacity 6):
  - priorities (`SoundPrio`): Warning 4 > Caution 3 > Chime 2 > Sample 1;
  - the same clip at the same priority is de-duplicated;
  - `pop(now)` drops entries older than 5 s, a chime older than 2 s (FR-002, FR-009);
  - `shouldPreempt(playingPrio, newPrio)` = new > playing.
- **Playback loop:**
  1. Take `RailLease(AudioAmp)` before the first clip and hold it across back-to-back clips.
  2. Wait **30 ms** for amplifier settling, then write **20 ms of zeros** (pop guard, FR-004).
  3. Open `/audio/<name>.wav` from FFat and parse the header with `core::Wav`. Reject anything but PCM16 mono 8–48 kHz
     → log once per clip, skip (FR-015).
  4. Read 512-sample chunks, apply the gain (`core::AudioMath::applyGain`, q15), and `board().audioWrite`.
  5. Between chunks, check the stop/preempt flags.
  6. At the end: 20 ms of zeros. If the queue is empty, wait a **200 ms tail** (catches a quick follow-up sound),
     then release the lease. Amplifier off ≤0.5 s after the last clip (FR-003).
- **Stop:** `AudioService::stop(minPrio)` sets a flag, and the loop exits within one chunk (512 / 16 kHz = 32 ms ≪
  0.5 s, FR-007). `stopAlerts()` = stop prio ≥ Caution and purge those from the queue.
- **Volume:** `volume` 0..4 → q15 gains {0, 0.30, 0.50, 0.71, 1.0}, roughly −10.5 / −6 / −3 / 0 dB plus Off.
  (First design {0, 0.18, 0.35, 0.6, 1.0}: the wearer found step 2 too quiet at T016; step 4 was fine.) 0 = Off:
  `play()` refuses and the amplifier stays off.
- **Output unavailable** (speaker not `Ok`, or `audioBegin` false): drop the queue, count it as skipped, release
  the lease, log once per boot. No retry loop. Alerts still vibrate (spec edge case).
- **Memory:** one 1 KB sample buffer (internal RAM, the I2S DMA source) plus the file handle. No PSRAM needed.

## R5 · Alert callouts (FR-005..FR-007)
- `core::Alerts` gains `enum class Clip : uint8_t { None, MasterCaution, MasterWarning, Stall, PullUp, BingoFuel,
  Minimums, Traffic, Chime }`. The S4 `voiceClip` field is filled in:
  - Stall → Stall, PullUp → PullUp, Test1..3 → Minimums;
  - `calloutFor(id, level)`: Advisory → None **first** (whatever the clip); then the alert's own clip; then
    Caution → MasterCaution, Warning → MasterWarning.
- **AlertManager wiring:**
  - on `Added`/`Rearmed` (not acknowledged) → `AudioService::play(callout, prio by level)`;
  - in the existing 2 s WARNING repeat: every **3rd** stick-shaker cycle (≈6 s) re-queues the warning callout.
    The counter resets whenever the shaker is (re)started by a raise or re-arm, so the first repeat is at ≈6 s;
  - `ackAll()` → `AudioService::stopAlerts()`.
- **Gates** (pure `core::AudioPolicy::allow(kind, quiet, volume, inQuietHours)`): quiet mode or volume Off → nothing;
  quiet hours → warnings only; otherwise everything.

## R6 · Hourly chime (FR-009, FR-010)
- Pure `core::ChimeRule::due(localSecOfDay, clockValid, lastChimeHourKey, settings) → (bool, hourKey)`:
  - `hourKey` = local epoch hours, so it is unique per hour;
  - due when the minute is 0, the second is < 5, `hourKey != lastChimeHourKey`, the chime is enabled, not quiet
    mode, volume > 0, the clock is valid, and not in quiet hours.
  - **Quiet hours** `[start, end)` in whole hours, wrapping when start > end (22 → 7); `start == end` = no quiet
    hours.
- **Duplicate protection:** `lastChimeHourKey` is stored in RAM. After a clock jump (sync/zone) into the first 5 s of
  an hour that already chimed, the key matches → no repeat. A jump over hours lands at a non-zero minute → no catch-up
  chime (FR-010).
- **Lateness:** the chime request goes stale after 2 s in the queue (`SoundQueue::maxAgeMs`), so with the 1 Hz
  tick a chime is heard within ≈2 s of hh:00 or not at all (FR-009, SC-003).
- **Test hook** `ad audio chimecheck HH:MM:SS`: applies that time to **today's local date** and uses the real
  `lastChimeHourKey`, so `already` appears only for an hour that actually chimed.
- Evaluated in AudioService's svc tick (1 Hz) from `ClockSnapshot` (UTC + `tzOffsetMin` → local).

## R7 · Settings (schema v6)
- `volume` U8 0..4 default 3 (was 2; the wearer found step 2 too quiet at T016);
- `chime` Bool default 1;
- `qhstart` U8 0..23 default 22;
- `qhend` U8 0..23 default 7.

All are editable and the migration is additive. Quiet mode is the existing S4 `quiet` key.

**Settings page "SOUND" section:**
- Volume dropdown Off/1/2/3/4;
- Hourly chime switch;
- Quiet hours: two hour dropdowns ("22:00" … "07:00");
- "Play sample" button (plays the chime at the current volume).

## R8 · Performance
- **FFat reads** are SPI flash reads through the cache. They can briefly stall the other core's flash/PSRAM accesses.
  Chunks are small (1 KB), and the sound itself rate-limits them (32 KB/s), so the impact should be negligible.
  **Measured** with the PFD sweep during a 10 s callout loop (SC-005).
- **The audio task (prio 5 on core 0)** pre-empts svc/net while it computes gain on 512 samples (≈50 µs per chunk);
  otherwise it is blocked in `i2s_write`.
