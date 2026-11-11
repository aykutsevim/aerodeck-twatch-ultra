// Primary Flight Display: the home page (slice S1).
#pragma once
#include "core/PfdLayout.h"
#include "ui/Page.h"
#include "ui/widgets/AdiWidget.h"
#include "ui/widgets/EicasWidget.h"
#include "ui/widgets/TapeWidget.h"

class PfdPage : public Page {
public:
    PageId id() const override { return PageId::Home; }
    const char* title() const override { return "PFD"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override;
    void onExit() override;

private:
    static void frameCb(lv_timer_t* t);
    static void renderReadyCb(lv_event_t* e);
    void frame(bool force);
    lv_obj_t* corner(lv_obj_t* scr, const core::Box& b, const char* caption, lv_obj_t** value);
    lv_obj_t* smallLabel(lv_obj_t* scr, const core::Box& b, const char* text, lv_align_t align);

    core::PfdLayout layout_{};
    AdiWidget adi_;
    TapeWidget heading_, hour_, minute_;
    EicasWidget eicas_;
    lv_obj_t* zulu_ = nullptr;
    lv_obj_t* local_ = nullptr;
    lv_timer_t* frame_ = nullptr;
    uint32_t lastNotifiedSampleMs_ = 0;
    int lastMinuteKey_ = -1;     // utc minute + validity + tz, to detect corner/tape changes
    uint32_t lastPowerSeq_ = 0;
};
