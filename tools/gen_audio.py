#!/usr/bin/env python3
"""Generate AeroDeck's audio clips into data/audio/ (slice S5, research R3).

Voice callouts: Windows text-to-speech (System.Speech, offline, built into Windows), voice
"Microsoft Zira Desktop" (fallback "Microsoft Hazel Desktop"). The cabin chime is synthesized here.
No third-party recordings are used (constitution VI).

Every clip: WAV, PCM 16-bit, mono, 16 kHz; leading/trailing silence trimmed (-45 dBFS),
peak-normalised (callouts -1 dBFS, chime -3 dBFS), 10 ms fade-in/out.

Usage (Windows):  python tools/gen_audio.py
Then flash with:  pio run -e twatch_ultra -t uploadfs   (ADR-013)
"""
import array
import datetime
import math
import os
import subprocess
import sys
import tempfile
import wave

RATE = 16000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "data", "audio")
VOICES = ["Microsoft Zira Desktop", "Microsoft Hazel Desktop"]

# file name -> (spoken text, speaking rate -10..10)
PHRASES = [
    ("master_caution", "Master caution", 1),
    ("master_warning", "Master warning", 1),
    ("stall", "Stall. Stall.", 2),
    ("pull_up", "Pull up. Pull up.", 2),
    ("bingo_fuel", "Bingo fuel", 1),
    ("minimums", "Minimums", 1),
    ("traffic", "Traffic. Traffic.", 2),
]

PS_TEMPLATE = r"""
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$wanted = @({voices})
$installed = @($s.GetInstalledVoices() | ForEach-Object {{ $_.VoiceInfo.Name }})
$v = $null
foreach ($w in $wanted) {{ if ($installed -contains $w) {{ $v = $w; break }} }}
if ($v -eq $null) {{ Write-Output "NOVOICE"; exit 2 }}
$s.SelectVoice($v)
$s.Rate = {rate}
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo({hz}, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$s.SetOutputToWaveFile('{path}', $fmt)
$s.Speak('{text}')
$s.SetOutputToNull()
$s.Dispose()
Write-Output $v
"""


def tts(text, rate, path):
    script = PS_TEMPLATE.format(voices=", ".join("'%s'" % v for v in VOICES), rate=rate, hz=RATE,
                                path=path.replace("'", "''"), text=text.replace("'", "''"))
    r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                       capture_output=True, text=True)
    out = r.stdout.strip().splitlines()
    if r.returncode != 0 or not out or out[-1] == "NOVOICE":
        sys.exit("TTS failed for %r: %s %s" % (text, r.stdout, r.stderr))
    return out[-1]


def read_pcm(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2 or w.getnchannels() != 1 or w.getframerate() != RATE:
            sys.exit("unexpected TTS format in %s" % path)
        a = array.array("h")
        a.frombytes(w.readframes(w.getnframes()))
        if sys.byteorder == "big":
            a.byteswap()
        return list(a)


def trim(s, thr_dbfs=-45.0, pad_ms=15):
    thr = 32767.0 * 10 ** (thr_dbfs / 20.0)
    idx = [i for i, v in enumerate(s) if abs(v) > thr]
    if not idx:
        return s
    pad = RATE * pad_ms // 1000
    return s[max(0, idx[0] - pad):min(len(s), idx[-1] + pad + 1)]


def normalise(s, peak_dbfs):
    peak = max(1, max(abs(v) for v in s))
    g = 32767.0 * 10 ** (peak_dbfs / 20.0) / peak
    return [v * g for v in s]


def fade(s, ms=10):
    n = min(len(s) // 2, RATE * ms // 1000)
    s = list(s)
    for i in range(n):
        k = i / float(n)
        s[i] *= k
        s[-1 - i] *= k
    return s


def write_wav(path, s):
    a = array.array("h", [max(-32768, min(32767, int(round(v)))) for v in s])
    if sys.byteorder == "big":
        a.byteswap()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(a.tobytes())
    return len(a) / float(RATE)


def chime():
    """Two-tone "bing-bong": 880 Hz then 660 Hz, 350 ms apart, each ringing out with exponential decay."""
    total = int(RATE * 1.05)
    s = [0.0] * total
    for start_s, f in ((0.0, 880.0), (0.35, 660.0)):
        t0 = int(start_s * RATE)
        for i in range(total - t0):
            t = i / float(RATE)
            env = min(1.0, t / 0.005) * math.exp(-t / 0.22)
            # a little 2nd and 3rd harmonic for a bell-like timbre on a small speaker
            v = (math.sin(2 * math.pi * f * t) + 0.25 * math.sin(4 * math.pi * f * t)
                 + 0.08 * math.sin(6 * math.pi * f * t))
            s[t0 + i] += env * v
    return fade(normalise(s, -3.0), 10)


def main():
    if os.name != "nt":
        sys.exit("gen_audio.py needs Windows (System.Speech).")
    os.makedirs(OUT, exist_ok=True)
    voice_used = None
    lines = []
    with tempfile.TemporaryDirectory() as tmp:
        for name, text, rate in PHRASES:
            raw = os.path.join(tmp, name + ".raw.wav")
            voice_used = tts(text, rate, raw)
            s = fade(normalise(trim(read_pcm(raw)), -1.0), 10)
            d = write_wav(os.path.join(OUT, name + ".wav"), s)
            lines.append((name, text, d))
            print("%-15s %-20s %.2f s  (%s)" % (name, '"' + text + '"', d, voice_used))
    d = write_wav(os.path.join(OUT, "chime.wav"), chime())
    lines.append(("chime", "(synthesized two-tone chime)", d))
    print("%-15s %-20s %.2f s" % ("chime", "synth 880/660 Hz", d))

    with open(os.path.join(OUT, "README.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# AeroDeck audio clips\n\n")
        f.write("Generated by `tools/gen_audio.py` on %s. **Original text-to-speech and synthesis only; no "
                "third-party audio** (constitution VI).\n\n" % datetime.date.today().isoformat())
        f.write("- Engine: Windows `System.Speech.Synthesis.SpeechSynthesizer` (offline)\n")
        f.write("- Voice: %s\n" % voice_used)
        f.write("- Format: WAV, PCM 16-bit, mono, 16 kHz; trimmed, peak-normalised (-1 dBFS callouts, -3 dBFS "
                "chime), 10 ms fades\n")
        f.write("- Chime: synthesized, 880 Hz then 660 Hz 350 ms apart, exponential decay\n\n")
        f.write("| File | Content | Duration |\n|---|---|---|\n")
        for name, text, d in lines:
            f.write("| `%s.wav` | %s | %.2f s |\n" % (name, text, d))
        f.write("\nFlash to the watch with `pio run -e twatch_ultra -t uploadfs` (ADR-013). This replaces the "
                "whole FAT partition (the LilyGoLib factory MP3s are removed).\n")
    print("voice:", voice_used)


if __name__ == "__main__":
    main()
