"""
AeroDeck: reproducibly patch LilyGoLib 0.2.0 for Arduino-ESP32 2.x / ESP-IDF 4.4 (ADR-008).

LilyGoDispInterface.cpp guards IDF5-only esp_lcd fields with
`#if (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(4,0,0))`, which is true on
Arduino core 2.0.17 and breaks the build. This post: script runs after lib_deps
are installed and before compilation, and rewrites the guard to test the IDF version.
Idempotent: re-running detects the marker and does nothing.
"""
import os
import re

Import("env")  # noqa: F821  (provided by SCons)

MARKER = "AERODECK-PATCH-1"
REL = os.path.join("LilyGoLib", "src", "LilyGoDispInterface.cpp")

CANONICAL = (
    "    // " + MARKER + ": guard on ESP-IDF version (upstream checked the Arduino core version)\n"
    "#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)\n"
    "    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;\n"
    "    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;\n"
    "#else\n"
    "    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;\n"
    "#endif\n"
    "    panel_config.vendor_config = nullptr;\n"
)

# Original upstream block (outer #if ... nested #if/#else/#endif ... outer #endif)
ORIGINAL_RE = re.compile(
    r"#if \(ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL\(4,0,0\)\)\n"
    r".*?"
    r"#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL\(5, 0, 0\)\n.*?#endif\n#endif\n",
    re.S,
)
# Earlier manual patch made during bring-up (comment + simplified guard)
MANUAL_RE = re.compile(
    r"[ \t]*// Patched locally:.*?\n.*?panel_config\.vendor_config = nullptr;\n",
    re.S,
)


def log(msg):
    print("patch_lilygolib: " + msg)


def patch():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")  # noqa: F821
    pioenv = env.subst("$PIOENV")  # noqa: F821
    path = os.path.join(libdeps, pioenv, REL)
    if not os.path.isfile(path):
        log("ERROR: %s not found (is LilyGoLib in lib_deps?)" % path)
        env.Exit(1)  # noqa: F821
    with open(path, "r", encoding="utf-8", newline="") as f:
        src = f.read()
    text = src.replace("\r\n", "\n")
    if MARKER in text:
        log("already patched (%s)" % pioenv)
        return
    new, n = ORIGINAL_RE.subn(CANONICAL, text, count=1)
    if n == 0:
        new, n = MANUAL_RE.subn(CANONICAL, text, count=1)
    if n == 0:
        log("ERROR: expected guard not found in %s. LilyGoLib changed? Update tools/patch_lilygolib.py" % path)
        env.Exit(1)  # noqa: F821
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(new)
    log("patched (%s)" % pioenv)


# AERODECK-PATCH-2 (S7): SensorLib 0.3.4 BoschParseCallbackManager::call(sensor_id, data, uint32_t size, ...)
# shadows the member `size` (entry count) with the data length, so with USE_CUSTOM_VECTOR the loop reads
# uninitialised malloc'd entries past the end and may call a garbage callback (crash in parseData once 6-byte
# accelerometer events arrive, heap-layout dependent). Loop over the entry count instead.
MARKER2 = "AERODECK-PATCH-2"
REL2 = os.path.join("SensorLib", "src", "bosch", "BoschParseCallbackManager.hpp")
BUG2 = (
    "    void call(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp)\n"
    "    {\n"
    "#ifdef USE_CUSTOM_VECTOR\n"
    "        for (uint32_t i = 0; i < size; i++) {\n"
)
FIX2 = (
    "    void call(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp)\n"
    "    {\n"
    "#ifdef USE_CUSTOM_VECTOR\n"
    "        // " + MARKER2 + ": `size` (data length) shadowed the entry count\n"
    "        for (uint32_t i = 0; i < this->size; i++) {\n"
)


def patch_sensorlib():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")  # noqa: F821
    pioenv = env.subst("$PIOENV")  # noqa: F821
    path = os.path.join(libdeps, pioenv, REL2)
    if not os.path.isfile(path):
        log("SensorLib not in this env; skipping patch 2 (%s)" % pioenv)
        return
    with open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read().replace("\r\n", "\n")
    if MARKER2 in text:
        log("SensorLib already patched (%s)" % pioenv)
        return
    if BUG2 not in text:
        log("ERROR: SensorLib callback loop not found in %s. SensorLib changed? Update tools/patch_lilygolib.py" % path)
        env.Exit(1)  # noqa: F821
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text.replace(BUG2, FIX2, 1))
    log("SensorLib patched (%s)" % pioenv)


patch()
patch_sensorlib()
