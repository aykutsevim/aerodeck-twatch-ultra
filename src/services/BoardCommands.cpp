#include "services/BoardCommands.h"
#include <cstdio>
#include <cstring>
#include "board/IBoard.h"
#include "core/SimFail.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include <Arduino.h>
#include "services/Log.h"

using namespace core;

namespace BoardCommands {

static void cmdPeriph(int, const char* const*, Reply& r)
{
    r.ok();
    uint32_t caps = board().caps();
    (void)caps;
    char b[64];
    for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
        PeriphState s = board().periph(Periph(i));
        snprintf(b, sizeof b, "%s present=%d state=%s", periphName(Periph(i)), s == PeriphState::Absent ? 0 : 1,
                 periphStateName(s));
        r.line(b);
    }
}

static void cmdSimfail(int argc, const char* const* argv, Reply& r)
{
    uint16_t mask = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "none") == 0) {
            mask = 0;
            continue;
        }
        int bit = SimFail::bitForName(argv[i]);
        if (bit < 0) {
            r.err("unknown peripheral (lora nfc gnss sd mic haptics imu rtc none)");
            return;
        }
        mask |= uint16_t(1u << bit);
    }
    SettingsStore::set(SettingKey::SimFail, mask, true);
    char b[32];
    snprintf(b, sizeof b, "OK mask=0x%02x", mask);
    r.line(b);   // contract: single "OK mask=0x.." line; applies on next boot
}

// S7 T006 probe (test hook): deep sleep without the vendor delay, NVG flush filter.
static void cmdProbeS7(int, const char* const* argv, Reply& r)
{
    if (strcmp(argv[0], "sleep") == 0) {
        r.ok();
        r.line("sleeping; press the button to wake");
        LOGI("probe", "deepSleep call at %lu ms", (unsigned long)millis());
        Serial.flush();
        delay(50);
        if (!board().deepSleep(core::kWakeButton, 0)) {
            LOGW("probe", "deepSleep unsupported");
        }
    } else if (strcmp(argv[0], "nvg") == 0 || strcmp(argv[0], "day") == 0) {
        board().setNvgFilter(strcmp(argv[0], "nvg") == 0);
        r.ok();
    } else if (strcmp(argv[0], "i2c") == 0) {
        uint8_t a[32];
        const uint8_t n = board().i2cScan(a, sizeof a);
        char b[160];
        size_t k = size_t(snprintf(b, sizeof b, "i2c:"));
        for (uint8_t i = 0; i < n && k + 6 < sizeof b; i++) {
            k += size_t(snprintf(b + k, sizeof b - k, " 0x%02X", a[i]));
        }
        r.ok();
        r.line(b);
    } else if (strcmp(argv[0], "wake") == 0) {
        r.ok();
        r.line(core::wakeCauseName(board().wakeCause()));
    } else {
        r.err("expected sleep|nvg|day|wake (display: use `ad screen off|on`, UI task)");
    }
}

void registerCommands()
{
    SerialConsole::add("probe s7", 1, cmdProbeS7);
    SerialConsole::add("periph", 0, cmdPeriph);
    SerialConsole::add("simfail", 1, cmdSimfail);
}

} // namespace BoardCommands
