// Drop-tank chronograph page (slice S8, FR-001…004, FR-010…012): tank gauge, MM:SS, phase/cycles, presets 5/15/25/50,
// custom −/+, Countdown/Pomodoro, break 5/10/15, Start/Pause and Reset. Reads ChronoSnapshot; commands go to
// ChronoService (svc task); lengths and mode are settings (chrlen/chrmode/chrwork/chrbrk).
#pragma once
#include <cstdint>
#include "services/ChronoService.h"
#include "ui/Page.h"
#include "ui/widgets/TankWidget.h"

class ChronoPage : public Page {
public:
    PageId id() const override { return PageId::Chrono; }
    const char* title() const override { return "CHRONO"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override { refresh(true); }
    void onTick(uint32_t dtMs) override;

private:
    static void presetCb(lv_event_t* e);
    static void stepCb(lv_event_t* e);
    static void modeCb(lv_event_t* e);
    static void breakCb(lv_event_t* e);
    static void toggleCb(lv_event_t* e);
    static void resetCb(lv_event_t* e);
    void refresh(bool force);
    void setLocked(bool locked);

    TankWidget tank_;
    lv_obj_t* time_ = nullptr;
    lv_obj_t* phase_ = nullptr;
    lv_obj_t* len_ = nullptr;
    lv_obj_t* modeBtn_ = nullptr;
    lv_obj_t* modeLbl_ = nullptr;
    lv_obj_t* brkBtn_ = nullptr;
    lv_obj_t* brkLbl_ = nullptr;
    lv_obj_t* goLbl_ = nullptr;
    lv_obj_t* edit_[7] = {};   // controls locked while running/paused
    uint32_t lastSeq_ = 0;
    char timeText_[12] = "";
    char phaseText_[40] = "";
    char lenText_[24] = "";
    bool locked_ = false;
};
