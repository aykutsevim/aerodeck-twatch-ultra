# AeroDeck — Product Roadmap

| Field | Value |
| :--- | :--- |
| Date | 2026-11-10 (status updated 2026-11-11) |
| Target | LilyGO T-Watch Ultra (primary), T-Watch S3 (secondary) |
| Method | spec-kit, one feature branch per slice (`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`) |
| References | AD-SRS-001, AD-SAS-001, `speckit_slice_prompts.md` |

## 1. Vision
A daily-wearable "glass cockpit" watch that puts every sensor and radio on the T-Watch to work, with the attention to detail of real avionics. It should be fun to show off, readable at a glance, and frugal with battery.

## 2. Guiding principles
1. **Every slice ships.** Each slice ends with firmware you can flash and wear, and never leaves the watch broken.
2. **Foundation before features.** Navigation, theming, power leases and the alert pipeline come before the exotic modules.
3. **Battery is a feature.** Peripherals are leased, not left on.
4. **Measure, then promise.** FPS, current draw and battery life are measured at milestones and written into the docs.
5. **Experimental work is clearly labelled** and stays behind feature flags.

## 3. Milestones & slices

```
M0 Foundation   ── S0
M1 Wearable MVP ── S1 → S2 → S3
M2 Cockpit Alive ─ S4 → S5 → S6
M3 Daily Driver ── S7 → S8
M4 Tactical      ─ S9 → S10 → S11
M5 Experimental ── S12 , S13
```

| Slice | Name | Milestone | Depends on | Priority | Size | Exit criteria (demo) | Status |
| :---: | :--- | :---: | :--- | :---: | :---: | :--- | :--- |
| **S0** | Platform foundation | M0 | — | Must | M | Clean-checkout build incl. LilyGoLib patch; HAL; page ring with 2 placeholder pages + Settings; NVS settings; rail leases; FPS/heap overlay; native test env green | ✅ Done (2026-11-11, PR #1) · manual: T032, T039, T063 |
| **S1** | PFD watch face | M1 | S0 | Must | L | Horizon follows wrist at ≥30 FPS, LOCK indicator, H/M/S tapes, Zulu/Local, EICAS from PMU | ✅ Done (2026-11-11, PR #2) · manual: T014, T018, T034, T043 |
| **S2** | Time sync & connectivity | M1 | S0 | Must | M | Provision Wi-Fi; NTP sync at boot + hourly writes RTC; correct TZ; Wi-Fi off after sync | ✅ Done (2026-11-11, PR #3) · manual: T017, T020, T026 (SC-004), T038 (24-h soak) |
| **S3** | METAR weather page | M1 | S2 | Must | M | LTAC METAR decoded and shown with flight category; STALE/error handling; parser unit tests | ✅ Done (2026-11-11, PR #4) · manual: T023 (no-internet ×10, Wi-Fi forgotten), T028, T036 (24-h soak) |
| **S4** | Alert Manager & haptics | M2 | S0 | Must | M | Master annunciator; click haptics on nav; <15 % battery → stick shaker + STALL; ack works | ✅ Done (2026-11-11, PR #5) · wearer checks confirmed by user (not script-logged) |
| **S5** | GPWS audio | M2 | S4 | Must | M | WAV callouts from the FAT partition (`uploadfs`, ADR-013); hourly chime with quiet hours; amp rail off when idle | ✅ Done (2026-11-12, PR #6) · wearer listening checks recorded in validation.md; no manual checks pending |
| **S6** | G-force & flight recorder | M2 | S1 | Must | M | Daily NM + Max G on PFD and Recorder page; 7-day history survives reboot | ✅ Done (2026-11-12, PR #7) · manual: T015 SC-001 (3 walks ≥500 m, NM within ±15 %) |
| **S7** | NVG, eject & power UX | M3 | S4, S6 | Must | M | NVG theme on all pages; Pull-to-Eject deep sleep + wake; screen timeout + raise-to-wake; **battery-life measurement report** | ✅ Done (2026-11-13, PR #8) · ≈34 mA avg / ≈33 h, NFR-PWR-002 met; horizon tilt added (FR-PFD-012); wrist-motion wake not possible (ADR-014) · manual: T020 raise false-wake walk, T042 fuel-gauge cross-check |
| **S8** | Drop-tank chronograph | M3 | S4, S5, S7 | Must | S | Countdown/Pomodoro with depleting tank; Bingo Fuel alert fires even after sleep | ⏳ Planned |
| **S9** | TCAS LoRa radar | M4 | S4, S7 | Should | L | Two watches (or watch + node) see each other on the PPI; region/duty-cycle enforced | ⏳ Planned |
| **S10** | PTT: dB meter & acoustic commands | M4 | S4 | Should | M | Hold-to-talk dB gauge; whistle / double-clap trigger actions | ⏳ Planned |
| **S11** | IFF countermeasures | M4 | S4 | Could | S | Guarded FIRE cover; Ultra: NFC NDEF action; S3: IR code send | ⏳ Planned |
| **S12** | GNSS enhancements | M5 | S2, S9 | Could | M | GPS page; optional true course on heading tape; GNSS time fallback; TCAS true bearing | ⏳ Planned |
| **S13** | LoRa walkie-talkie (Codec2) | M5 | S9, S10 | Could (experimental) | L | Spike first; 3 s voice clip sent and played between two watches | ⏳ Planned |

### Status tracking rule

**After each slice finishes (its PR is merged into `main`), its row in the table above MUST be updated in the same
or the very next change.** Record:
- the status: ✅ Done, with the merge date and PR number;
- any on-device checks still pending (task IDs from that slice's `tasks.md`).

When a slice's spec work starts, mark it 🚧 In progress with its branch name. When a milestone's definition of done
is met, mark the milestone ✅ Reached with the date (table in §4).

Legend: ✅ Done / Reached · 🚧 In progress · ⏳ Planned · ⏸ Parked.

Sizes: S ≈ 2–4 dev-days, M ≈ 1 week, L ≈ 2 weeks (hobby pace, adjust as velocity becomes known).

## 4. Milestone definitions of done
| Milestone | Definition of done | Status |
| :--- | :--- | :--- |
| **M0 Foundation** | Architecture skeleton merged. `pio run -e twatch_ultra -e twatchs3` and `pio test -e native` pass. Constitution ratified. | ✅ Reached (2026-11-11, S0 merged) |
| **M1 Wearable MVP** | The watch is usable as a daily watch: accurate time (NTP), the PFD face, and weather at a glance. | ✅ Done (2026-11-11): S1, S2, S3 merged · manual checks pending as listed per slice |
| **M2 Cockpit Alive** | Alerts, haptics, voice and the activity recorder work together. Low battery is unmistakable. | ✅ Done (2026-11-12): S4, S5, S6 merged · manual checks pending as listed per slice |
| **M3 Daily Driver** | Battery life is measured and meets NFR-PWR-002 (average ≤50 mA, ≥22 h typical, radios off). Night use is comfortable. Timer is reliable. | ⏳ S7 done (battery target met, NVG); S8 timer pending |

*Amendment (2026-11-13):* the NFR-PWR-002 target was redefined from ≥48 h to an average drain of ≤50 mA
(≥22 h on the 1100 mAh battery) after the S7 measurement; no further power optimisation is planned for now
(constitution 1.0.4, SRS NFR-PWR-002, `docs/power_report.md`). Measured: ≈ 33 h, so the target is met.
| **M4 Tactical** | All hardware is in use. Two-device TCAS demo recorded. | ⏳ |
| **M5 Experimental** | GNSS features and voice link are evaluated. Each is either promoted to Should or parked with findings. | ⏳ |

## 5. Hardware deltas vs. concept spec (decisions already taken)
| Concept spec item | Roadmap decision |
| :--- | :--- |
| T-Watch S3, 240×240 LCD | Primary target is the T-Watch Ultra, 410×502 AMOLED. Resolution-independent UI keeps S3 viable. |
| LVGL 8.3 | LVGL 9.4 (already pinned). |
| BMA423 / PCF8563 | BHI260AP / PCF85063A behind the HAL. |
| IR LED countermeasures | The Ultra has no IR LED, so the Ultra uses NFC for IFF (S11). IR remains for S3 builds. |
| 60 FPS locked | 30 FPS guaranteed, 60 FPS stretch, measured in S1. |
| 915 MHz | Region-configurable. Default EU868 (Türkiye). |
| — | New, Ultra-only: GNSS (S12), NFC (S11), microSD (optional storage). |

## 6. Risks
| Risk | Likelihood | Impact | Mitigation |
| :--- | :---: | :---: | :--- |
| Horizon too slow at 410×502 | Med | High | Measure early in S1. Transform-based rendering, partial invalidation, canvas fallback. |
| LilyGoLib/IDF 4.4 incompatibilities beyond the known patch | Med | Med | S0 automates patching. Arduino 3.x migration slice as plan B. |
| Deep-sleep wake from IMU unsupported on Ultra wiring | Med | Low | Button + timer wake fallback (SRS OI-1). |
| LoRa duty-cycle limits make TCAS sluggish | Med | Low | 30–60 s beacons, RSSI smoothing. Beacons are tiny. |
| Codec2 integration cost | High | Low | S13 gated by spike. Already marked experimental. |
| Audio assets licensing | Low | Med | Use self-recorded or TTS clips only. |

## 7. Backlog / ideas (not scheduled)
- Arduino-ESP32 3.x / IDF 5 migration (removes the LilyGoLib patch, enables `ESP_I2S`).
- OTA updates over Wi-Fi.
- BLE companion: phone notifications as "ATC messages".
- Flight-log export to microSD (CSV) or over USB.
- Meshtastic passive decode (FR-TCS-007) promoted to Should.
- Additional watch faces: HSI, analog "six-pack" round dials.
