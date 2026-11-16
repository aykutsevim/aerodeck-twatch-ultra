# ADR-015: LoRa in EU868 on 915 MHz hardware; 1 % self-imposed duty budget (closes SRS OI-6)

- **Status:** Accepted (slice S9, 2026-11-16)
- **Relates to:** constitution Principles III (battery) and VI (deliberate, compliant, private);
  AD-SAS-001 §4, §7.2, §7.3; SRS FR-TCS-001/008, NFR-REG-001, OI-6; ADR-006;
  spec `specs/010-tcas-lora-radar` (research R1, R4)

## Context

The wearer's T-Watch Ultra is the **915 MHz** variant: its SX1262 matching network, filter and antenna are tuned
for 902–928 MHz. The wearer is in **Türkiye**, where that band is not available for this use. The SRD band
**863–870 MHz** (ERC Rec 70-03) is. The SX1262 itself tunes from 150 to 960 MHz.

Regulations govern what is emitted (frequency, power, duty cycle), not how well the antenna is matched. Transmitting
at 868 MHz on 915-tuned hardware is legal; it only costs range and efficiency.

## Decision

1. **Region EU868 is the default and the only region used on the device.** The region table narrows EU868 to
   sub-band **g3** (869.40–869.65 MHz) on **one fixed channel, 869.525 MHz**. The frequency is shown, not edited.
2. **Profile:** SF9 / BW125 / CR4/5, private sync word 0x12, 8-symbol preamble. Power **2–14 dBm, default
   10 dBm**, lower than the legal limit because the hardware is not matched for this band.
3. **Duty budget:** **1 % per rolling hour (36 s)**, enforced in software before every transmit
   (`core::DutyCycle`). This is stricter than g3's 10 % and holds on any EU868 sub-band. The history lives in RTC
   memory, so eject and soft resets cannot reset the budget.
4. **US915** stays in the table (902.3–914.9 MHz, ≤ 20 dBm, 400 ms dwell, the same 1 % budget) for use where it
   is legal. It is selected only with an explicit `ad set lorareg 1`, never by default, and not from the page.
5. The beacon (SAS §7.2) carries only version, node id, sequence, callsign and battery %; no position in S9.

## Consequences

- **Range is shorter** than with 868 MHz hardware, and RSSI reads lower, so contacts appear on outer rings at a given
  distance. The ring constants (SAS §7.3) are kept; the effect is documented in the validation.
- Measured on the device: transmit-done for every beacon at 869.525 MHz; airtime 192–193 ms against 185 ms computed.
- One beacon (18 B) takes about 185 ms, so the default 30 s interval uses about 0.6 % of the hour. The 15 s minimum
  interval would need about 1.2 %; the budget then delays or skips beacons.
- Moving to an 868 MHz unit needs no software change.
