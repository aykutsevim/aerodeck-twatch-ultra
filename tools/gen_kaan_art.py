#!/usr/bin/env python3
"""Generate AeroDeck's KAAN art assets (feature 011, research R4, contracts/art_assets.md).

Source: art/kaan-splash-screen.jpg (white top-view KAAN silhouette + "KAAN" lettering on black, from the wearer).
Output: src/assets/KaanArt.h / KaanArt.cpp: two 8-bit alpha masks (A8):
  kKaanSplash  full-screen splash (fits the 410x502 rounded screen with a margin)
  kKaanEmblem  PFD emblem (fits PfdLayout's emblem box between ZULU and LOCAL)

The target sizes are computed for the T-Watch Ultra (410x502, corner radius 64). For another screen, change
SCREEN_W/SCREEN_H/CORNER_R and re-run. The firmware places the emblem from core::PfdLayout and skips it where
the box does not exist.

JPEG decoding and resampling use .NET System.Drawing through Windows PowerShell (bicubic, built into Windows,
no third-party packages), like tools/gen_audio.py uses Windows TTS. The generated files are committed. The
firmware build needs neither this tool nor the JPEG. Output is deterministic.

Usage (Windows):  python tools/gen_kaan_art.py [--emblem-aircraft-only]
"""
import argparse
import math
import os
import subprocess
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "art", "kaan-splash-screen.jpg")
OUT_H = os.path.join(ROOT, "src", "assets", "KaanArt.h")
OUT_CPP = os.path.join(ROOT, "src", "assets", "KaanArt.cpp")

SCREEN_W, SCREEN_H, CORNER_R = 410, 502, 64
SPLASH_MARGIN = 8          # px kept clear inside the rounded screen
BBOX_LUMA = 48             # content threshold for the crop box
CROP_MARGIN = 0.04         # 4 % of the crop size on every side
BLACK, WHITE = 16, 235     # luminance -> alpha mapping
PROXY_DIV = 4              # bounding box found on a 1/4-size proxy

PS = r"""
param([string]$src, [int]$cx, [int]$cy, [int]$cw, [int]$ch, [int]$ow, [int]$oh, [string]$out)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($src)
$bmp = New-Object System.Drawing.Bitmap($ow, $oh, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Black)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
$ia = New-Object System.Drawing.Imaging.ImageAttributes
$ia.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
$dst = New-Object System.Drawing.Rectangle(0, 0, $ow, $oh)
$g.DrawImage($img, $dst, $cx, $cy, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel, $ia)
$g.Dispose()
$r = New-Object System.Drawing.Rectangle(0, 0, $ow, $oh)
$d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $bmp.PixelFormat)
$n = $d.Stride * $oh
$buf = New-Object byte[] $n
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $n)
$bmp.UnlockBits($d)
[System.IO.File]::WriteAllBytes($out, $buf)
"{0} {1}" -f $img.Width, $img.Height
$bmp.Dispose(); $img.Dispose()
"""


def resample(crop, ow, oh):
    """crop=(x,y,w,h) of the source, resampled to ow x oh. Returns (luma rows, source size)."""
    with tempfile.TemporaryDirectory() as td:
        ps1 = os.path.join(td, "rs.ps1")
        raw = os.path.join(td, "out.raw")
        with open(ps1, "w", encoding="utf-8") as f:
            f.write(PS)
        cx, cy, cw, ch = crop
        res = subprocess.run(
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1, SRC, str(cx), str(cy),
             str(cw), str(ch), str(ow), str(oh), raw], capture_output=True, text=True, check=True)
        sw, sh = (int(v) for v in res.stdout.split()[-2:])
        data = open(raw, "rb").read()
    stride = (ow * 3 + 3) & ~3
    rows = []
    for y in range(oh):
        o = y * stride
        row = []
        for x in range(ow):
            b, g, r = data[o + 3 * x], data[o + 3 * x + 1], data[o + 3 * x + 2]
            row.append((299 * r + 587 * g + 114 * b + 500) // 1000)
        rows.append(row)
    return rows, (sw, sh)


def to_alpha(rows):
    out = []
    for row in rows:
        for v in row:
            a = (v - BLACK) * 255 // (WHITE - BLACK)
            out.append(0 if a < 0 else 255 if a > 255 else a)
    return out


def inside_rounded(x, y, w, h, r):
    cx = r if x < r else (w - r if x > w - r else x)
    cy = r if y < r else (h - r if y > h - r else y)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def fits_screen(a8, aw, ah):
    """Every alpha>0 pixel (both of its far corners) inside the rounded screen shrunk by SPLASH_MARGIN."""
    x0, y0 = (SCREEN_W - aw) // 2, (SCREEN_H - ah) // 2
    W, H, R = SCREEN_W - 2 * SPLASH_MARGIN, SCREEN_H - 2 * SPLASH_MARGIN, max(CORNER_R - SPLASH_MARGIN, 0)
    for y in range(ah):
        for x in range(aw):
            if a8[y * aw + x]:
                for px, py in ((x0 + x, y0 + y), (x0 + x + 1, y0 + y + 1)):
                    if not inside_rounded(px - SPLASH_MARGIN, py - SPLASH_MARGIN, W, H, R):
                        return False
    return True


def sc(s, px):
    return int(px * s + 0.5)


def emblem_height():
    """Same formula as core::PfdLayout portrait() for the Ultra: h - m - rowY (data-model.md)."""
    w, h, r = SCREEN_W, SCREEN_H, CORNER_R
    s = min(w, h) / 240.0
    m, headH, eicasH, tapeW, timeH, bannerH = sc(s, 4), sc(s, 24), sc(s, 36), sc(s, 40), sc(s, 34), sc(s, 28)
    a = w - 2 * (tapeW + 2 * m)
    a = min(a, sc(s, 140)) & ~1
    contentW = a + 2 * (tapeW + m)
    x0 = (w - contentW) // 2
    totalH = headH + m + a + m + eicasH + m + timeH
    y0 = max((h - totalH) // 2, bannerH)
    inset = min(x0, w - (x0 + tapeW))
    if inset < r:
        d = r - inset
        y0 = max(y0, int(math.ceil(r - math.sqrt(r * r - d * d))))
    if y0 + totalH > h - m:
        y0 = h - m - totalH
    y0 = max(y0, 0)
    rowY = y0 + headH + m + a + m + eicasH + m
    return h - m - rowY, contentW


def content_box(aircraft_only):
    pw, ph = 1696 // PROXY_DIV, 2528 // PROXY_DIV
    rows, (sw, sh) = resample((0, 0, 1696, 2528), pw, ph)
    assert (sw, sh) == (1696, 2528), (sw, sh)
    if aircraft_only:   # blank the "KAAN" lettering column (upper left), keep the wings below it
        for y in range(int(ph * 0.526)):
            for x in range(int(pw * 0.354)):
                rows[y][x] = 0
    xs = [x for row in rows for x, v in enumerate(row) if v > BBOX_LUMA]
    ys = [y for y, row in enumerate(rows) if any(v > BBOX_LUMA for v in row)]
    x0, x1 = min(xs) * PROXY_DIV, (max(xs) + 1) * PROXY_DIV
    y0, y1 = min(ys) * PROXY_DIV, (max(ys) + 1) * PROXY_DIV
    mx, my = int((x1 - x0) * CROP_MARGIN), int((y1 - y0) * CROP_MARGIN)
    x0, y0 = max(0, x0 - mx), max(0, y0 - my)
    x1, y1 = min(sw, x1 + mx), min(sh, y1 + my)
    return (x0, y0, x1 - x0, y1 - y0)


def c_array(name, a8, w):
    lines = ["static const uint8_t %s[%d] = {" % (name, len(a8))]
    for i in range(0, len(a8), 32):
        lines.append("    " + ", ".join("0x%02X" % v for v in a8[i:i + 32]) + ",")
    lines.append("};")
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emblem-aircraft-only", action="store_true")
    args = ap.parse_args()

    crop = content_box(False)
    aspect = crop[2] / crop[3]

    # Splash: largest size with the crop's aspect that fits the rounded screen with the margin.
    sh_ = SCREEN_H - 2 * SPLASH_MARGIN
    while True:
        sw_ = int(round(sh_ * aspect))
        if sw_ > SCREEN_W - 2 * SPLASH_MARGIN:
            sw_ = SCREEN_W - 2 * SPLASH_MARGIN
            sh_ = int(round(sw_ / aspect))
        rows, _ = resample(crop, sw_, sh_)
        splash = to_alpha(rows)
        if fits_screen(splash, sw_, sh_):
            break
        sh_ -= 4

    # Emblem: the PfdLayout box height; width by aspect, capped at contentW/4.
    ecrop = content_box(True) if args.emblem_aircraft_only else crop
    easpect = ecrop[2] / ecrop[3]
    eh, contentW = emblem_height()
    ew = int(round(eh * easpect))
    if ew > contentW // 4:
        ew = contentW // 4
        eh = int(round(ew / easpect))
    erows, _ = resample(ecrop, ew, eh)
    emblem = to_alpha(erows)

    hdr = [
        "// GENERATED by tools/gen_kaan_art.py from art/kaan-splash-screen.jpg. Do not edit.",
        "// Source 1696x2528; crop box x=%d y=%d w=%d h=%d (content + 4 %% margin)." % crop,
        "// Splash %dx%d A8 (fits the 410x502 r=64 screen with %d px margin); emblem %dx%d A8%s." % (
            sw_, sh_, SPLASH_MARGIN, ew, eh, " (aircraft only)" if args.emblem_aircraft_only else ""),
    ]
    h = hdr + [
        "#pragma once",
        '#include "core/AlphaArt.h"',
        "",
        "extern const AlphaArt kKaanSplash;   // full-screen splash (feature 011)",
        "extern const AlphaArt kKaanEmblem;   // PFD emblem between ZULU and LOCAL",
        "",
    ]
    cpp = hdr + ['#include "assets/KaanArt.h"', ""]
    cpp += c_array("kSplashA8", splash, sw_) + [""]
    cpp += c_array("kEmblemA8", emblem, ew) + [""]
    cpp += [
        "const AlphaArt kKaanSplash = {%d, %d, kSplashA8};" % (sw_, sh_),
        "const AlphaArt kKaanEmblem = {%d, %d, kEmblemA8};" % (ew, eh),
        "",
    ]
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    for path, lines in ((OUT_H, h), (OUT_CPP, cpp)):
        with open(path, "w", encoding="utf-8", newline="\r\n") as f:
            f.write("\n".join(lines))
    print("crop", crop, "splash %dx%d emblem %dx%d (emblem box height %d, contentW/4 %d)" % (
        sw_, sh_, ew, eh, emblem_height()[0], contentW // 4))


if __name__ == "__main__":
    main()
