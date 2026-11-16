// External drop-tank gauge (S8, FR-002): a horizontal capsule tank with a pointed nose and tail fins; the fuel level
// falls with the remaining fraction. UI task only. Colours from Theme (the NVG flush filter handles night mode).
#pragma once
#include <lvgl.h>

class TankWidget {
public:
    void create(lv_obj_t* parent, int32_t w, int32_t h);
    void set(float fraction);   // 0..1; redraws only when the 1 % step changes

private:
    static void onDraw(lv_event_t* e);
    lv_obj_t* obj_ = nullptr;
    int pct_ = -1;
};
