#include "core/LogFormat.h"
#include <cstdio>
#include <cstring>

namespace core {

char lvlChar(Lvl l)
{
    static const char k[] = {'E', 'W', 'I', 'D'};
    uint8_t i = uint8_t(l);
    return i < sizeof k ? k[i] : '?';
}

size_t formatLogLine(char* out, size_t n, uint32_t ms, Lvl lvl, const char* tag, const char* msg)
{
    if (!out || n == 0) {
        return 0;
    }
    int w = snprintf(out, n, "[%lu][%c][%s] %s", (unsigned long)ms, lvlChar(lvl), tag ? tag : "",
                     msg ? msg : "");
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    return size_t(w) < n ? size_t(w) : n - 1;
}

} // namespace core
