# Generating the HUD digit fonts

`src/ui/fonts/dseg7_40.c` and `dseg7_28.c` are generated files, committed so the build needs no network or Node
(constitution VII). Regenerate only if sizes or glyphs change.

| Item | Value |
| :--- | :--- |
| Font | DSEG7 Classic Bold, keshikan/DSEG release **v0.46** |
| Archive | `fonts-DSEG_v046.zip` from https://github.com/keshikan/DSEG/releases/download/v0.46/fonts-DSEG_v046.zip |
| Archive SHA-256 | `a6c2f43520971ca8067262e78d49025e605f749bf716ec5394bad9a0ee1c238c` |
| Licence | SIL Open Font License 1.1, copied to `src/ui/fonts/OFL.txt` (from `DSEG-LICENSE.txt`) |
| Converter | `lv_font_conv@1.5.3` (npm), run with Node 24 |
| Glyphs | `0x20` space, `0x2D` -, `0x2E` ., `0x30-0x3A` 0-9 and : |

```sh
unzip fonts-DSEG_v046.zip
for N in 40 28; do
  npx -y lv_font_conv@1.5.3 --font fonts-DSEG_v046/DSEG7-Classic/DSEG7Classic-Bold.ttf \
    --range 0x20,0x2D,0x2E,0x30-0x3A --size $N --bpp 4 --format lvgl --no-compress \
    --lv-include lvgl.h -o src/ui/fonts/dseg7_$N.c
done
```
