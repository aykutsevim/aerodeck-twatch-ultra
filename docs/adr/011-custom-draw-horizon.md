# ADR-011: Custom-drawn horizon, size cap and stepped heading tape for ≥30 FPS

- **Status:** Accepted (slice S1, 2026-11-11). The size cap and stepped tape were chosen by the product owner after
  the FPS gate (task T016/T028) failed.
- **Refines:** AD-SAS-001 §8 (horizon rendering)

## Context
The constitution (Principle IV) and NFR-PERF-001 require the PFD horizon to sustain ≥30 FPS on the Ultra
(410×502). S0 measured full-screen page transitions at 15–28 FPS. LilyGoLib's LVGL helper renders through two
internal-RAM DMA buffers of 1/6 screen; PSRAM full-frame buffers measured far slower (5–7 FPS).

## Decision
1. **Custom draw, no transforms.** The ADI is one `lv_obj` with an `LV_EVENT_DRAW_MAIN` handler:
   - the sky is the object's background;
   - the ground is the square clipped by the horizon half-plane, filled as a triangle fan;
   - pitch ladder lines (every 5°, ±20°) with labels at 10°/20°;
   - fixed aircraft symbol and roll pointer.
   It is invalidated only on a change ≥0.2° (or a lock/status change).
2. **Size cap ≈238 px** (`PfdLayout`: `min(fit, S(140))`) instead of the ~280 px that fits.
3. **Heading (seconds) tape in 1° steps** (≈6 redraws/s, one per 1/6 s) instead of every frame.
4. Display refresh period 16 ms (S0). DMA partial buffers unchanged; **no further vendor patch**.

## Evidence (on device, synthetic motion `ad att sweep on`, overlay FPS over 60 s)

| Configuration | FPS |
| :--- | :--- |
| ADI only, 280 px | 32–34 |
| Heading tape only, every frame | 62 |
| ADI 280 + heading every frame (full PFD) | 22–25 (median 23) |
| …same, element switched off: ladder labels / ladder lines / whole ladder / ground / symbol / horizon line | 25 / 25 / 27 / 25 / 23 / 24 |
| …same, backgrounds only (all vector elements off) | 34 (pipeline ceiling) |
| ADI 238 + heading every frame | 26 |
| **ADI 238 + heading in 1° steps** (ladder ±25°) | **median 30, avg 29.4, min 28** |
| ADI 238 + 1° heading + ladder ±20° (final) | **median 30, avg 29.5** (isolated dips to 10–14 coincide with serial polling) |
| Refresh period 10 ms instead of 16 ms | no gain (median 30) |

The "backgrounds only" row shows that the limit is the per-frame pixel volume through LilyGoLib's display
pipeline, not the vector drawing. Element removal gains ≤5 FPS.

## Consequences
- SC-001 is met at the threshold (median 30). There is no headroom, so later PFD additions must not increase the
  per-frame dirty area.
- The instrument is ~15 % smaller than the space allows. The seconds scroll moves in visible 1° steps.
- If more headroom is needed later: larger DMA buffers (needs touching LilyGoLib's LVGL helper, i.e. widening ADR-008's
  patch) or the Arduino-ESP32 3.x / IDF 5 migration (SRS OI-5), which may bring faster LCD paths.

## Alternatives
- Object/layer transforms: LVGL 9 renders transformed objects through an intermediate layer, which is more expensive.
- `lv_canvas` in PSRAM: PSRAM rendering was the slow path in S0.
- Bigger DMA buffers via LVGL's API from board code: viable, but LilyGoLib's own buffers can't be freed without
  touching the vendor helper. Deferred.
- Amending the 30 FPS rule: rejected by the product owner.
