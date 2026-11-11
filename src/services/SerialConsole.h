// AeroDeck USB serial console (`ad ...` commands, contracts/serial_commands.md). Runs on the svc task.
#pragma once
#include "core/CommandParser.h"

namespace SerialConsole {
bool add(const char* pattern, uint8_t minArgs, core::Handler fn);
void begin();                 // registers built-ins and the svc tick
void tick(uint32_t nowMs);    // also callable from a headless idle loop
void setSafeModeFlag(bool safe);
} // namespace SerialConsole
