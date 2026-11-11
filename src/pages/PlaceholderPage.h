// Placeholder MFD page used to exercise navigation in S0.
#pragma once
#include "ui/Page.h"

class PlaceholderPage : public Page {
public:
    PlaceholderPage(PageId id, const char* title) : id_(id), title_(title) {}
    PageId id() const override { return id_; }
    const char* title() const override { return title_; }
    void onCreate(lv_obj_t* screen) override;

private:
    PageId id_;
    const char* title_;
};
