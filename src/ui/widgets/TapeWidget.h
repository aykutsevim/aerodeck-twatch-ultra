// Instrument tapes (research R3). Vertical: hour/minute with neighbour values and a value box.
// Horizontal: heading tape, custom-drawn, scrolled every frame from the fractional seconds. UI task only.
#pragma once
#include <lvgl.h>

class TapeWidget {
public:
    enum class Dir { Vertical, Horizontal };

    // modulo: value range for wrap-around of the neighbour labels (24 for hours, 60 for minutes).
    void create(lv_obj_t* parent, Dir dir, int32_t x, int32_t y, int32_t w, int32_t h, int modulo,
                const char* caption);
    void setValue(int v);          // vertical; -1 shows "--"
    void setHeading(float deg);    // horizontal; < 0 shows "---" and freezes the scale
    lv_obj_t* obj() const { return obj_; }

private:
    static void onDraw(lv_event_t* e);
    void drawHeading(lv_layer_t* layer);

    Dir dir_ = Dir::Vertical;
    lv_obj_t* obj_ = nullptr;
    lv_obj_t* box_ = nullptr;
    lv_obj_t* boxLbl_ = nullptr;
    lv_obj_t* neigh_[4] = {};   // vertical: v+2, v+1, v-1, v-2 (top to bottom)
    int modulo_ = 60;
    int value_ = -2;            // force first update
    float heading_ = -2.0f;
};
