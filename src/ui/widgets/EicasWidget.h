// Engine indications strip: FUEL QTY, FUEL FLOW, EGT and the CHG annunciator (FR-012..FR-016). UI task.
#pragma once
#include <lvgl.h>
#include "services/PowerService.h"

class EicasWidget {
public:
    void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    void set(const PowerSnapshot& p);

private:
    lv_obj_t* cell(lv_obj_t* parent, const char* caption, lv_obj_t** value);
    lv_obj_t* qty_ = nullptr;
    lv_obj_t* flow_ = nullptr;
    lv_obj_t* egt_ = nullptr;
    lv_obj_t* chg_ = nullptr;
    char last_[3][16] = {};
    int lastChg_ = -1;
};
