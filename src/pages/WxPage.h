// WX page (S3): decoded METAR for the configured station, flight category, wind compass, raw text, STALE/error
// annunciators and a REFRESH button (contracts/wx_page.md). Reads Snapshot<WeatherSnapshot>; UI task only.
#pragma once
#include "services/WeatherService.h"
#include "ui/Page.h"

class WxPage : public Page {
public:
    PageId id() const override { return PageId::Weather; }
    const char* title() const override { return "WX"; }
    void onCreate(lv_obj_t* screen) override;
    void onEnter() override { update(true); }
    void onTick(uint32_t dtMs) override;   // polls the snapshot seq; STALE re-check every 30 s

    void refreshTapped();

private:
    void update(bool force);
    void updateCompass(const WeatherSnapshot& s);

    lv_obj_t* stn_ = nullptr;
    lv_obj_t* obs_ = nullptr;
    lv_obj_t* pill_ = nullptr;
    lv_obj_t* pillTxt_ = nullptr;
    lv_obj_t* annun_ = nullptr;
    lv_obj_t* upd_ = nullptr;
    lv_obj_t* wind_ = nullptr;
    lv_obj_t* vis_ = nullptr;
    lv_obj_t* qnh_ = nullptr;
    lv_obj_t* tdp_ = nullptr;
    lv_obj_t* clouds_ = nullptr;
    lv_obj_t* raw_ = nullptr;
    lv_obj_t* rawBox_ = nullptr;
    lv_obj_t* btn_ = nullptr;
    // compass
    lv_obj_t* rose_ = nullptr;
    lv_obj_t* arrow_ = nullptr;
    lv_obj_t* head_ = nullptr;
    lv_obj_t* sector_ = nullptr;
    lv_obj_t* centre_ = nullptr;
    lv_point_precise_t arrowPts_[2] = {};
    lv_point_precise_t headPts_[3] = {};
    int32_t roseD_ = 0;

    uint32_t lastSeq_ = 0;
    uint32_t staleAcc_ = 0;
    bool refreshing_ = false;
    uint32_t refreshSeq_ = 0;
    bool noWifiFlash_ = false;
    bool created_ = false;
};
