// Settings page: brightness, overlay, log level, restore defaults, About/Status (FR-008, FR-012, FR-014).
// S4 ALERTS section; S5 SOUND section (volume, hourly chime, quiet hours, Play sample).
#pragma once
#include "ui/Page.h"

class SettingsPage : public Page {
public:
    PageId id() const override { return PageId::Settings; }
    const char* title() const override { return "SETTINGS"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override { refresh(); }
    void onTick(uint32_t dtMs) override;   // refreshes "Last sync" every 10 s
    void refresh();   // re-read cache + peripheral status (UI task)

private:
    lv_obj_t* brightVal_ = nullptr;
    lv_obj_t* overlaySw_ = nullptr;
    lv_obj_t* logDd_ = nullptr;
    lv_obj_t* tzVal_ = nullptr;      // S2: zone name (button label)
    lv_obj_t* wxVal_ = nullptr;      // S3: weather station (button label)
    lv_obj_t* vibDd_ = nullptr;      // S4: vibration off/low/high
    lv_obj_t* quietSw_ = nullptr;    // S4: quiet mode
    lv_obj_t* volDd_ = nullptr;      // S5: volume Off/1-4
    lv_obj_t* chimeSw_ = nullptr;    // S5: hourly chime
    lv_obj_t* qhStartDd_ = nullptr;  // S5: quiet hours start (00:00..23:00)
    lv_obj_t* qhEndDd_ = nullptr;    // S5: quiet hours end
    lv_obj_t* netVal_ = nullptr;     // S2: network name
    lv_obj_t* netStatus_ = nullptr;  // S2: last result / busy
    lv_obj_t* lastSync_ = nullptr;   // S2: time since last successful sync
    uint32_t tickAcc_ = 0;
    lv_obj_t* statusBox_ = nullptr;
    bool created_ = false;
};
