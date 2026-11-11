// Placeholder Home page (S0). Replaced by the PFD watch face in slice S1.
#pragma once
#include "ui/Page.h"

class HomePage : public Page {
public:
    PageId id() const override { return PageId::Home; }
    const char* title() const override { return "HOME"; }
    void onCreate(lv_obj_t* screen) override;
    void onTick(uint32_t dtMs) override;

private:
    lv_obj_t* clock_ = nullptr;
    uint32_t lastSec_ = UINT32_MAX;
};
