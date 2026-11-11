// Settings page: brightness, overlay, log level, restore defaults, About/Status (FR-008, FR-012, FR-014).
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
    lv_obj_t* netVal_ = nullptr;     // S2: network name
    lv_obj_t* netStatus_ = nullptr;  // S2: last result / busy
    lv_obj_t* lastSync_ = nullptr;   // S2: time since last successful sync
    uint32_t tickAcc_ = 0;
    lv_obj_t* statusBox_ = nullptr;
    bool created_ = false;
};
