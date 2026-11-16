# Research: TCAS LoRa Radar (Slice S9)

**Date**: 2026-11-16 · **Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md)

Sources:
- LilyGoLib 0.2.0 `LilyGoWatchUltra` (`SX1262 radio` global from RadioLib 7.x, `initLoRa()` = `radio.begin()` with
  defaults, `setRFSwitch(false)` = built-in antenna, `lockSPI()/unlockSPI()` for the shared SPI bus);
- the Ultra pins (`LORA_CS 36`, `LORA_RST 47`, `LORA_BUSY 48`, `LORA_IRQ 14`; SPI shared);
- AD-SAS-001 §7.2 (beacon v1) and §7.3 (RSSI rings);
- AeroDeck `core::Alerts::calloutFor`.

---

## R1 — Region, band and power on 915 MHz hardware in Türkiye

**Facts:**
- The unit is the **915 MHz** variant: the wearer read it from the label, which closes OI-6.
- In Türkiye (and under the EU SRD rules, ERC Rec 70-03) the usable LoRa band is **863–870 MHz**. The 902–928 MHz
  band is not available.
- The SX1262 tunes from 150 to 960 MHz, so it *can* transmit at 868 MHz. The board's matching network, filter and
  antenna are tuned for 915 MHz, which costs some range and efficiency at 868. That is a performance issue, not a
  legality issue: the emitted frequency and power are what the regulations govern.

**Decision:**
- Region **EU868 is the default and the only region enabled by default.** The region table deliberately narrows
  EU868 to sub-band g3 (869.40–869.65 MHz), one fixed channel, rather than the whole 863–870 MHz band.
- Profile **869.525 MHz** (sub-band g3, 869.40–869.65 MHz: 10 % duty, ≤ 500 mW ERP), **SF9 / BW125 / CR4/5**,
  private sync word 0x12, 8-symbol preamble.
- The power limit is enforced at **2–14 dBm**, default **10 dBm**: lower because the hardware is not tuned for this
  band.
- The **duty-cycle budget is 1 % per rolling hour**. That is stricter than the 10 % allowed in g3, as written in
  NFR-REG-001, and leaves a margin on any sub-band.
- **US915** stays in the region table for users where it is legal:
  - 902.3–914.9 MHz;
  - ≤ 20 dBm;
  - a **400 ms maximum dwell** per transmission (FCC §15.247 for non-hopping systems);
  - the same 1 % self-imposed budget;
  - selectable only by an explicit `lorareg` change.
- The page and the console show the region and frequency.
- **Rationale:** constitution VI requires region-compliant RF use. Operating in the legal band at a modest power is
  the compliant choice for this hardware and location.
- **Alternatives:**
  - *US915 at 915 MHz:* illegal in Türkiye. Rejected.
  - *Disabling TCAS on 915 MHz hardware:* too conservative; 868 MHz is legal on this chip.

## R2 — Radio driver location and threading

**Decision:**
- `IBoard` gets a small radio API, implemented in `BoardUltra` with LilyGoLib's `radio` object (RadioLib `SX1262`):
  - `radioStart(const RadioProfile&)`: `radio.begin(freq, bw, sf, cr, sync, power, preamble)`,
    `setRFSwitch(false)`, `setCurrentLimit`, `setDio1Action(isr)`, `startReceive()`. Runs under `lockSPI()`.
  - `radioStop()`: `radio.sleep()`, clears the ISR.
  - `radioSend(buf, n)`: `startTransmit()`, then the caller blocks on the DIO1 semaphore without the SPI lock (≈ 0.2 s; 1.3 s at SF12), `finishTransmit()`, then back to `startReceive()`. (Validation Q7: the busy-polling `transmit()` starved the IMU task at SF12.) Returns the actual
    airtime (`getTimeOnAir`).
  - `radioPoll(Packet&)`: when the DIO1 flag is set, `readData()` plus RSSI/SNR, then `startReceive()` again.
  - `radioAirtimeMs(n)`.
- The radio **rail** (`Rail::Lora`) is held by `TcasService` with a `RailLease` while TCAS is on (constitution III).
  The chip loses its configuration when unpowered, so `radioStart` runs after every rail-on.
- **Threading:** a dedicated **`radio` task** (core 0, priority 4, 4 KB) runs RX polling (woken by the DIO1 ISR
  through a task notification) and transmits. It talks to `TcasService` through queues. The svc tick is never
  blocked by a 0.2 s transmit.
- **Rationale:**
  - LilyGoLib already owns the module wiring and the RF switch on the XL9555.
  - RadioLib 7.1.2/7.8.1 is pinned (S0).
  - A separate task keeps transmits off the svc tick (constitution IV and the 50 ms chrono tick).

## R3 — Beacon format (SAS §7.2, v1)

**Decision:** the beacon follows SAS §7.2 exactly:
- `AD 01 | type 01 | nodeId u32 | seq u8 | callsign[8] | batt u8 | flags u8`: **18 bytes** without position;
  flags bit0 = 0;
- `nodeId` = the low 32 bits of the eFuse MAC;
- the decoder rejects a wrong magic, a wrong length, an unknown type, or a non-printable callsign;
- a `has-position` packet (24 B) decodes the position, but it is unused until S12.

Pure `core::BeaconCodec`. A Meshtastic hook (`core::BeaconCodec::isMeshtastic(buf)`, always false for now) is left
for FR-TCS-007.

**Airtime:** SF9 BW125 CR4/5, 18 B payload, 8-symbol preamble, explicit header, CRC on ≈ **185 ms**, computed by
pure `core::LoraAirtime` (Semtech formula) and cross-checked against RadioLib's `getTimeOnAir` on the device.

## R4 — Duty-cycle budget

**Decision:** pure `core::DutyCycle`:
- a sliding 1-hour window of (start ms, airtime ms) records, a ring of 256;
- `canSend(now, airtimeMs)`: the sum over the window + airtime ≤ the budget (1 % → **36 000 ms/h**);
- `record(now, airtimeMs)`.

At the 15 s minimum interval (240 beacons × 185 ms ≈ 44 s/h) the budget is the limit. The service then **delays**
the beacon to the next possible time (checked each tick), counts `skipped` when it is more than one interval late,
and logs it. US915 also checks a dwell of ≤ 400 ms per transmission.

**Persistence:** the record ring is kept in RTC slow memory (`RTC_NOINIT_ATTR`, with a magic and a CRC), so it
survives eject (deep sleep) and soft resets; repeated reboots cannot reset the budget. Records are time-stamped in
**UTC ms from the system clock** (set from the RTC at boot, S8), not `millis()`, which restarts at every boot. On
restore, a record dated in the future (the clock was set backwards) is clamped to now, which is conservative. After a
power-on reset the ring is invalid and starts empty; nothing was transmitted while unpowered anyway.

## R5 — RSSI → ring, bearing, ageing

**Decision:** pure `core::TcasTable` (max 16 contacts):
- `update(nodeId, callsign, rssi, snr, batt, nowMs)`;
- an EMA on RSSI (α = 0.3);
- `ring = clamp((−40 − rssiEma) / 15, 0, 3)` (SAS §7.3; 4 rings, 0 = inner);
- **bearing** = a stable hash of the node id → 0–359°;
- `age(nowMs, timeoutMs)` drops stale contacts;
- `enteredInner` is set when the ring becomes 0 from a ring > 0, or from new. It is re-armed when the contact
  leaves the inner ring or ages out (FR-010).
- When the table is full, the oldest contact is replaced.

**Note:** the 915 MHz hardware at 868 MHz makes RSSI lower than on matched hardware, so rings read "farther". The
constants are kept and validation documents it.

## R6 — Alerts

**Decision:**
- New `AlertId::Traffic` = 44: **Advisory**, `Pattern::Advisory`, clip `Clip::Traffic` (already in the S5 clip
  enum), text "TRAFFIC". The clip is queued at `SoundPrio::Caution` (the existing AlertManager rule for non-Warning
  levels).
- **Screen wake:** `ScreenPolicy::onAlert` wakes only for Caution/Warning today. It gains a `speaks` flag: an
  advisory that speaks (only TRAFFIC) wakes the screen too. AlertManager publishes the flag in the `AlertRaised`
  event (`b` = level, plus a bit for "speaks"), and main.cpp passes it on.
- `core::calloutFor` changes: an Advisory speaks **only** when its definition has an explicit clip (only TRAFFIC
  has one). All existing advisories stay silent.
- Quiet mode and quiet hours still apply (the S5 policy).

## R7 — Page

**Decision:** `TcasPage` (a new `PageId::Tcas` after Chrono):
- a custom-drawn **PPI** in a square box: 4 range rings, a centre ownship symbol, and a **sweep line** with a
  fading trail drawn at 20 Hz. It is redrawn only while the page is visible.
- **Bogeys** are small clickable objects (≥ 48 px hit area via `ext_click_area`) placed at (bearing, ring radius).
- A **detail card** shows callsign, RSSI, SNR, "heard N s ago" and battery.
- A header shows the own callsign, region/frequency, contact count and the TCAS on/off toggle. With TCAS off it
  shows "TCAS OFF"; on the S3 or with a radio failure it shows "RADIO N/A".

The PPI is invalidated at 20 Hz only while visible. The PFD is not affected (FR-013).

## R8 — Power

- TCAS on means continuous RX: the SX1262 at about 5 mA plus about 0.2 s of transmit (about 45 mA at 10 dBm) every
  30 s, an average of about 5.3 mA.
- **Measured on the device with the USB meter** (FR-014), screen off. Added to `docs/power_report.md`. TCAS stays
  off by default.
- **RX duty cycling** (RadioLib `startReceiveDutyCycleAuto`, which needs longer preambles) is noted as a later
  optimisation; it is not in this slice (no power optimisation now, wearer 2026-11-13).

## R8b — Verifying with one device

- **TX:** the radio reports transmit-done for each beacon; counters and airtime are logged; the duty budget is
  checked over a 1-hour soak.
- **Rail:** `ad pwr log` shows `lora:1` only while TCAS is on.
- **RX path:**
  - The receiver runs (`startReceive` OK, RX timeouts and CRC errors counted).
  - **Foreign packets** with our sync word are rare. To still exercise the receive path, `ad tcas rxprobe` sets the
    public sync word 0x34 for 60 s (LoRaWAN/TTN traffic at 868.1–868.5 MHz is common in cities) and counts
    preambles, headers and packets. This is listen-only: nothing is transmitted on other channels.
- **Radar logic:** simulated contacts via `ad tcas sim <id> <callsign> <rssi>` feed the same table and page path as
  real packets.
