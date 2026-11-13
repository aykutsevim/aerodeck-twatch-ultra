// Pull-to-Eject handle (S7, FR-006/007, research R8): yellow/black hazard stripes across the top band of the
// watch face. Double tap → ARMED (≤ 3 s) → pull it down ≥ 40 % of the travel → EjectRequested. UI task only.
#pragma once
#include <lvgl.h>
#include "core/EjectGesture.h"

class EjectHandle {
public:
    void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t travel);

private:
    static void onEvent(lv_event_t* e);
    static void onDraw(lv_event_t* e);
    static void onTimer(lv_timer_t* t);
    void sync(uint32_t now);
    void showState();

    core::EjectGesture g_;
    lv_obj_t* obj_ = nullptr;
    lv_obj_t* plate_ = nullptr;
    lv_obj_t* label_ = nullptr;
    int32_t y0_ = 0;
    core::EjectGesture::State shown_ = core::EjectGesture::State::Idle;
    bool flash_ = false;
};
