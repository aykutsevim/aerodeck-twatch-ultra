#include "core/Zones.h"
#include "core/PosixTz.h"

namespace core {
namespace Zones {

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

int find(const char* name)
{
    if (!name) {
        return -1;
    }
    for (size_t i = 0; i < count(); i++) {
        const char* a = at(i).name;
        const char* b = name;
        while (*a && *b && lower(*a) == lower(*b)) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            return int(i);
        }
    }
    return -1;
}

static bool fixedMatch(size_t i, int16_t offsetMin)
{
    PosixTz t;
    return t.parse(at(i).posix) && !t.hasDst() && t.stdOffsetMin() == offsetMin;
}

size_t bestMatch(int16_t offsetMin)
{
    if (fixedMatch(defaultIndex(), offsetMin)) {
        return defaultIndex();
    }
    for (size_t i = 0; i < count(); i++) {   // named first, then fixed offsets
        if (fixedMatch(i, offsetMin)) {
            return i;
        }
    }
    return defaultIndex();
}

} // namespace Zones
} // namespace core
