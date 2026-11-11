# ADR-009: Use the FAT (`ffat`) partition for assets instead of LittleFS

- **Status:** Accepted (slice S0, 2026-11-11)
- **Amends:** AD-SAS-001 §7.1

## Context
AD-SAS-001 §7.1 planned a LittleFS partition for audio assets (slice S5). LilyGoLib 0.2.0 already mounts
**FAT (`FFat`) on the partition named `ffat`** inside `LilyGoUltra::begin()` (`setupMSC`), and exposes it
to the host computer as **USB mass storage**. The upstream `platformio.ini` also pointed
`board_build.partitions` into `.pio/libdeps/...`, which breaks on a clean checkout (constitution VII).

## Decision
- A project-owned `partitions/aerodeck_16MB.csv`, copied from LilyGoLib's factory layout: nvs 20 KB,
  otadata, app0/app1 4 MB each, **ffat ≈ 7.9 MB**, coredump 64 KB.
- Assets (e.g. `/audio/*.wav` in S5) live on the FAT partition. Settings stay in NVS.

## Consequences
- WAV files can be dragged onto the watch over USB. No `uploadfs` step is needed.
- No second filesystem and no `NO_INIT_FATFS` workaround.
- The FAT partition currently holds LilyGoLib factory MP3 files (seen in the boot listing). S5 decides
  whether to keep them.
- FAT is less power-loss tolerant than LittleFS. That is acceptable because assets are read-mostly, and
  settings use NVS (atomic per key).

## Alternatives
- LittleFS: would need `NO_INIT_FATFS` plus a second partition, for no benefit.
