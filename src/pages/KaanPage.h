// KAAN page (feature 011, user addition): the full-size KAAN silhouette art as the last page of the ring
// (after Settings), i.e. the page reached by swiping right on the PFD. Static, decorative; NVG via the frame filter.
#pragma once
#include "ui/Page.h"

class KaanPage : public Page {
public:
    PageId id() const override { return PageId::Kaan; }
    const char* title() const override { return "KAAN"; }
    void onCreate(lv_obj_t* screen) override;

private:
    lv_image_dsc_t dsc_{};
};
