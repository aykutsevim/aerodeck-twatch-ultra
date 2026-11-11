// Attitude director indicator: custom-drawn horizon (research R2, ADR-011). UI task only.
#pragma once
#include <lvgl.h>
#include "services/AttitudeService.h"

class AdiWidget {
public:
    void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t size);
    // Stores the attitude and invalidates only on a visible change (≥0.2°, lock or status change).
    // Returns true when a redraw was requested.
    bool set(const AttitudeSnapshot& a);
    lv_obj_t* obj() const { return obj_; }
    uint32_t drawnSampleMs() const { return drawnSampleMs_; }

private:
    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer);

    lv_obj_t* obj_ = nullptr;
    lv_obj_t* lock_ = nullptr;
    lv_obj_t* flag_ = nullptr;
    int32_t size_ = 0;
    float pitch_ = 0, roll_ = 0;
    bool level_ = false;
    AttStatus status_ = AttStatus::Align;
    bool first_ = true;
    uint32_t sampleMs_ = 0;
    uint32_t drawnSampleMs_ = 0;
};
