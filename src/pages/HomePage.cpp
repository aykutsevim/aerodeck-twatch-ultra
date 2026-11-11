#include "pages/HomePage.h"
#include <Arduino.h>
#include "ui/Theme.h"

void HomePage::onCreate(lv_obj_t* scr)
{
    lv_obj_t* t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, Theme::fontHuge(), 0);
    lv_obj_set_style_text_color(t, Theme::hudGreen(), 0);
    lv_label_set_text(t, "AERODECK");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -Theme::S(24));

    clock_ = lv_label_create(scr);
    lv_obj_set_style_text_font(clock_, Theme::fontTitle(), 0);
    lv_obj_set_style_text_color(clock_, Theme::fg(), 0);
    lv_obj_align(clock_, LV_ALIGN_CENTER, 0, Theme::S(16));
    onTick(0);

    lv_obj_t* hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(hint, Theme::dim(), 0);
    lv_label_set_text(hint, "UPTIME  -  swipe for pages");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, Theme::S(40));
}

void HomePage::onTick(uint32_t)
{
    uint32_t s = millis() / 1000;
    if (s == lastSec_) {
        return;
    }
    lastSec_ = s;
    lv_label_set_text_fmt(clock_, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600 % 100),
                          (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
}
