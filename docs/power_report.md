# AeroDeck Power Report (slice S7)

- **Date:** 2026-11-13
- **Firmware:** branch `008-nvg-eject-power`
- **Device:** T-Watch Ultra
- **Requirements:** NFR-PWR-002 (average drain ≤50 mA, ≥22 h typical use, radios off; redefined from ≥48 h in
  S7), NFR-PWR-004 (profiling mode), spec
  `specs/008-nvg-eject-power` FR-018/FR-019, SC-005/SC-006

## Verdict

**The target is met.** NFR-PWR-002 is an average drain of **≤50 mA**, i.e. **≥22 h** on the 1100 mAh battery
(1100 mAh ÷ 50 mA). With S7's screen timeout (15 s) and an overnight eject, the average is about **34 mA**, an
estimate of about **33 h**. Without the eject it is about 49 mA (≈ 22 h), just within the target. Before S7, with
the screen always on, it was about 62 mA (≈ 18 h, would not meet it).

The largest consumer is the watch **with the screen off**: the CPU stays awake at full speed and still draws about
48 mA from the battery. Further optimisation is **not planned for now** (wearer decision); the options are listed
at the end for later.

## Method

1. **Current measurement.** The AXP2101 power chip has **no battery-current sensor**, so the watch cannot measure
   its own current (research R9). Currents were measured with the wearer's **USB power meter** at 5 V, with the
   battery full and charging finished (`ad pwr log`: `pct=100 chg=0 vbus=1`). In that state the meter reads what
   the watch itself draws.
2. **Battery equivalent.** I_batt ≈ I_5V × 5 V / 3.85 V (nominal cell voltage), so ×1.30.
   - This assumes the same conversion efficiency from USB and from the battery.
   - USB-serial was connected in states 1–3, which adds the USB PHY's current. The estimate is therefore slightly
     pessimistic.
3. **Battery capacity:** 1100 mAh (provided by the wearer).
4. **Profiling mode** (FR-018): `ad pwr log on` prints once per second:

   ```text
   [pwr] prof t=2540 pct=100 mv=4145 chg=0 vbus=1 flow=n/a scr=on nvg=1 rails=lora:0,amp:0,gnss:0,nfc:0,wifi:0 ma=n/a
   ```

   `ma` stays `n/a` on the AXP2101. `flow` is the S1 fuel-flow estimate in %/h, available after a few minutes off
   USB.
5. **Fuel-gauge cross-check** (multi-hour discharge per state off USB): **pending**, a manual check (see the end).

## Measured currents

| # | State | 5 V (meter) | Battery equivalent |
| :-: | :--- | :-: | :-: |
| 1 | Watch face on, day mode, brightness 80 %, live horizon/tapes | 48 mA | ≈ 62 mA |
| 2 | Watch face on, **NVG** (10 % brightness) | 45 mA | ≈ 58 mA |
| 3 | **Screen off**, watch idle (S7 timeout) | 37 mA | ≈ 48 mA |
| 4 | **Ejected** (deep sleep) | < 1 mA (meter shows 0.000 A) | < 1.3 mA |

- The panel and rendering account for only about 11 mA of state 1 (state 1 − state 3). NVG saves a further 3 mA.
  The **always-on part is the awake CPU, the motion hub and the services**.
- **SC-006** (ejected ≤ 1/10 of screen-off idle): < 1 mA vs. 37 mA, so **met**, by more than 37×. The bound is set
  by the meter's resolution.

## Typical day and estimate

The usage profile is research R9 (radios off, hourly chime outside quiet hours):

| Period | Hours | State | Battery mAh |
| :--- | :-: | :--- | :-: |
| Worn, screen on (≈120 glances of 15 s + page use) | 1.5 | 1 | 93 |
| Worn, screen on in NVG (evening) | 0.5 | 2 | 29 |
| Worn, screen off | 14 | 3 | 673 |
| Overnight, ejected | 8 | 4 | ≤ 10 |
| **Total per day** | 24 | | **≈ 806 mAh** (average ≈ 34 mA) |

| Scenario | Average battery current | Battery life (1100 mAh) |
| :--- | :-: | :-: |
| S7 defaults + overnight eject (above) | ≈ 34 mA | **≈ 33 h** |
| S7 defaults, no eject (screen off overnight) | ≈ 49 mA | ≈ 22 h |
| Before S7 (screen always on, as S0–S6) | ≈ 62 mA | ≈ 18 h |
| **Target NFR-PWR-002** | **≤ 50 mA** | **≥ 22 h** (1100 mAh ÷ 50 mA) |

Both S7 scenarios meet the target: about 34 mA with the overnight eject and about 49 mA without. The margin
without the eject is small (≈ 1 mA), so habitual ejecting at night is what gives the comfortable ≈ 33 h.

## Main consumers and later options (not planned now)

1. **CPU awake while the screen is off (state 3, ≈ 48 mA).** This is the biggest lever.
   - Enable ESP-IDF power management with **automatic light sleep and frequency scaling while the screen is off**
     (`esp_pm_configure`, 80 MHz minimum, wake on the touch/PMU/sensor interrupts and the svc timer).
   - Expected: a few mA, which would put the profile at several days (≈ 4 days if the screen-off state reaches
     8 mA), even without the overnight eject.
   - Not planned now: the ≤50 mA target is met (wearer decision, 2026-11-13).
2. **Motion-hub rates while the screen is off.** Gravity runs at 100 Hz for the horizon. 25 Hz is enough for
   raise-to-wake.
3. **USB-serial.** On battery the USB PHY is idle. Re-measure state 3 with the fuel-gauge method to remove its
   share.
4. **Raise-to-wake false wakes** while walking add screen-on time. Their rate is still to be measured (pending
   manual check: 5-minute walk).

## Pending manual checks

- **Fuel-gauge discharge per state, off USB** (≥ 2 h each; ejected ≥ 24 h) to cross-check the USB-meter numbers.
- **Raise false-wake rate** during a 5-minute walk.
