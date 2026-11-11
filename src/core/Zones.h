// AeroDeck core: built-in time-zone table (research R3, data-model §2). Pure.
// Table data is generated into ZonesTable.cpp by tools/gen_zones.py.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

struct Zone {
    const char* name;    // display name, e.g. "Istanbul", "UTC+05:30"
    const char* posix;   // POSIX TZ rule string
};

namespace Zones {
size_t count();
size_t namedCount();      // named zones come first, then fixed offsets
size_t defaultIndex();    // "Istanbul"
const Zone& at(size_t i);
const char* tzdataVersion();
int find(const char* name);              // case-insensitive exact name; -1 if not found
// A zone whose offset equals offsetMin all year: the default zone if it matches, else the first named zone without
// summer time, else the fixed-offset entry. Used to migrate the S1 `tzoff` setting.
size_t bestMatch(int16_t offsetMin);
} // namespace Zones

} // namespace core
