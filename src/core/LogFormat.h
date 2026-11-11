// AeroDeck core: log line formatting and level filtering (host-tested).
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

enum class Lvl : uint8_t { E = 0, W = 1, I = 2, D = 3 };

// Writes "[<ms>][<L>][<tag>] <msg>" into out (always NUL-terminated when n > 0).
// Returns the number of characters written (excluding NUL).
size_t formatLogLine(char* out, size_t n, uint32_t ms, Lvl lvl, const char* tag, const char* msg);

// True if a message at `msg` level passes the `threshold` (E < W < I < D).
inline bool shouldLog(Lvl msg, Lvl threshold) { return uint8_t(msg) <= uint8_t(threshold); }

char lvlChar(Lvl l);

} // namespace core
