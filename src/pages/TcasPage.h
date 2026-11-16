// TCAS radar page (slice S9, FR-007…009, research R7): a plan-position indicator with 4 range rings, a rotating
// sweep (20 Hz, only while visible), tappable bogeys (ring from smoothed RSSI, stable hashed bearing) and a detail
// card. Header: own callsign, region/frequency, contacts, beacons sent; an ON/OFF control. Reads TcasSnapshot;
// on/off goes through the `tcas` setting. NVG comes from the frame filter (S7).
#pragma once
#include <cstdint>
#include "services/TcasService.h"
#include "ui/Page.h"

class TcasPage : public Page {
public:
    PageId id() const override { return PageId::Tcas; }
    const char* title() const override { return "TCAS"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override;
    void onExit() override;
    void onTick(uint32_t dtMs) override;

private:
    static void onPpiDraw(lv_event_t* e);
    static void onSweep(lv_timer_t* t);
    static void onBogey(lv_event_t* e);
    static void onCard(lv_event_t* e);
    static void onToggle(lv_event_t* e);
    void refresh(bool force);
    void layoutBogeys(const TcasSnapshot& s);
    void updateCard(const TcasSnapshot& s);

    lv_obj_t* header_ = nullptr;
    lv_obj_t* info_ = nullptr;
    lv_obj_t* ppi_ = nullptr;
    lv_obj_t* status_ = nullptr;
    lv_obj_t* toggleLbl_ = nullptr;
    lv_obj_t* bogey_[core::TcasTable::kMax] = {};
    lv_obj_t* bogeyLbl_[core::TcasTable::kMax] = {};
    uint32_t bogeyId_[core::TcasTable::kMax] = {};
    lv_obj_t* card_ = nullptr;
    lv_obj_t* cardLbl_ = nullptr;
    lv_timer_t* sweepTimer_ = nullptr;
    uint32_t selected_ = 0;      // node id shown on the card (0 = none)
    uint32_t lostAtMs_ = 0;      // the selected contact disappeared at (0 = present)
    uint32_t lastSeq_ = 0;
    uint32_t lastRefreshMs_ = 0;
    uint16_t sweepDeg10_ = 0;    // sweep angle × 10
    uint32_t sweepFrames_ = 0;   // redraws while visible (validation: ≥ 20 Hz)
    uint32_t sweepSinceMs_ = 0;
    bool active_ = false;        // TCAS running: draw the sweep
    char infoText_[64] = "";
    char statusText_[16] = "";
};
