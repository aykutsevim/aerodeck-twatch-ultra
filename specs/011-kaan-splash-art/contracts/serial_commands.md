# Contract: serial console changes

## `ad boot` (extended)

Before:

```text
boot_to_home_ms=3776 wake=poweron
```

After:

```text
boot_to_home_ms=3790 wake=poweron splash=1 splash_at_ms=2950 splash_cost_ms=31
boot_to_home_ms=1420 wake=button splash=0 skip=wake
```

- `splash` = 1 if the splash was pushed in this boot.
- `splash_at_ms`: when it became visible.
- The visible window is `boot_to_home_ms − splash_at_ms`.
- `skip` ∈ `wake|safe|none|nomem`, present only when `splash=0`:
  - `wake`: any deep-sleep wake;
  - `safe`: safe/headless mode;
  - `none`: no art linked in;
  - `nomem`: the PSRAM strip buffer could not be allocated.

## Boot log lines (new)

```text
[boot] splash shown at 2950 ms (31 ms)
[boot] splash skipped (wake)
```

No new commands.
