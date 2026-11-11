# Quickstart & Validation: Platform Foundation (S0)

## Prerequisites
- PlatformIO Core 6.x. On this machine `pio.exe` is blocked by Windows Application Control, so use
  `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …` (written as `pio` below).
- For native tests: MinGW-w64 GCC on PATH (e.g. MSYS2 `pacman -S mingw-w64-ucrt-x86_64-gcc`, then add
  `C:\msys64\ucrt64\bin` to PATH). Check with `gcc --version`.
- T-Watch Ultra on USB (it showed up as COM3 previously).

## V1. Clean-checkout build (FR-022, FR-023, SC-008/009)
```powershell
Remove-Item -Recurse -Force .pio          # simulate fresh clone
pio run -e twatch_ultra                   # expect SUCCESS; log shows "patch_lilygolib: patched"
pio run -e twatch_ultra                   # expect "patch_lilygolib: already patched"
pio run -e twatchs3                       # expect SUCCESS
pio test -e native                        # expect all tests PASSED
```

## V2. Flash & boot (US1, SC-001)
```powershell
pio run -e twatch_ultra -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```
Power-cycle 10×. Home is interactive in ≤4 s each time. Serial shows `ad info` boot banner.

## V3. Navigation (US1, SC-002, SC-003, SC-006)
1. Swipe left ×4: Home → A → B → Settings → Home. Swipe right once: Settings.
2. Page indicator appears ~1 s after each change.
3. From B press button → Home. Press again → B.
4. 50 quick swipes each way: no skips or freezes. Transitions ≤300 ms; record the `nav … fps` values from the log (SC-006).

## V4. Settings persistence (US2, SC-004)
1. Settings: brightness → lowest step (screen dims at once), overlay ON.
2. Power-cycle: both values kept. `ad set list` confirms.
3. `ad set bright 9` → `ERR out of range [0..4]`.
4. Restore defaults on watch → confirm dialog → Cancel (no change) → Confirm (defaults back).
5. Power-loss test (×3): change brightness and immediately pull USB/battery power. On reboot the
   value is either old or new, and the watch boots normally.

## V5. Graceful degradation (US3, SC-005)
```text
ad simfail lora
ad reboot          → Home within 4 s, amber "LORA FAIL" banner; `ad periph` shows lora state=SIMFAIL
ad simfail lora nfc gnss sd mic haptics imu rtc
ad reboot          → Home, banner cycles through all; Settings status lists all
ad simfail none
ad reboot          → no banner
```
Leave it running for 1 h with a failure simulated: `ad info` shows uptime ≥3600 s and reset
reason unchanged.

## V6. Diagnostics (US4)
- `ad log warn` → INFO lines stop. `ad log info` → they resume.
- Overlay ON: values update ≥1/s. `ad stat` heapmin stays ≥40 KB after 30 min of navigation (SC-007).

## V7. Power baseline (FR-017)
`ad periph` + serial log at boot show LoRa/GNSS/NFC/AudioAmp rails OFF after boot.
