// Flight recorder MFD page (slice S6, FR-REC-004): today's NM, steps, Max G with its time, and a 7-day bar chart
// (previous 6 days + today; tap a bar for that day). Replaces the S0 "PAGE B" placeholder.
#pragma once
#include <cstdint>
#include "core/DayLog.h"
#include "ui/Page.h"

class RecorderPage : public Page {
public:
    PageId id() const override { return PageId::Recorder; }
    const char* title() const override { return "RECORDER"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override { refresh(true); }
    void onTick(uint32_t dtMs) override;

private:
    static constexpr int kBars = 7;
    static void barClicked(lv_event_t* e);
    void refresh(bool force);
    void setText(lv_obj_t* l, char* cache, size_t n, const char* text);
    void showSelected();

    lv_obj_t* nm_ = nullptr;
    lv_obj_t* steps_ = nullptr;
    lv_obj_t* maxg_ = nullptr;
    lv_obj_t* note_ = nullptr;        // "Motion sensor unavailable"
    lv_obj_t* selected_ = nullptr;    // the tapped day's values
    lv_obj_t* slot_[kBars] = {};
    lv_obj_t* bar_[kBars] = {};
    lv_obj_t* wd_[kBars] = {};
    int32_t barMaxH_ = 0;
    int selectedIdx_ = 0;             // 0 = today (rightmost bar)
    uint32_t lastSeq_ = 0;
    core::DayRecord shown_[kBars] = {};
    uint8_t shownStride_ = 0;
    char nmText_[16] = "";
    char stepsText_[32] = "";
    char maxgText_[40] = "";
    char selText_[64] = "";
};
