// AeroDeck page base class (contracts/internal_interfaces.md). All methods run on the UI task.
#pragma once
#include <cstdint>
#include <lvgl.h>

enum class PageId : uint8_t { Home, PlaceholderA, PlaceholderB, Settings, Count };

class Page {
public:
    virtual PageId id() const = 0;
    virtual const char* title() const = 0;
    virtual void onCreate(lv_obj_t* screen) = 0;   // build widgets once
    virtual void onEnter() {}                       // acquire leases, start timers
    virtual void onExit() {}                        // MUST release all leases
    virtual void onTick(uint32_t dtMs) { (void)dtMs; }   // ≤10 Hz while visible
    virtual ~Page() = default;
};
