#include "services/BoardCommands.h"
#include <cstdio>
#include <cstring>
#include "board/IBoard.h"
#include "core/SimFail.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"

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

void registerCommands()
{
    SerialConsole::add("periph", 0, cmdPeriph);
    SerialConsole::add("simfail", 1, cmdSimfail);
}

} // namespace BoardCommands
