#include "ui/widgets/EicasWidget.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "ui/Theme.h"

lv_obj_t* EicasWidget::cell(lv_obj_t* parent, const char* caption, lv_obj_t** value)
{
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_height(c, lv_pct(100));
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* cap = lv_label_create(c);
    lv_obj_set_style_text_font(cap, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(cap, Theme::dim(), 0);
    lv_label_set_text(cap, caption);
    *value = lv_label_create(c);
    lv_obj_set_style_text_font(*value, Theme::fontBody(), 0);
    lv_obj_set_style_text_color(*value, Theme::hudGreen(), 0);
    lv_label_set_text(*value, "--");
    return c;
}

void EicasWidget::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t* strip = lv_obj_create(parent);
    lv_obj_remove_style_all(strip);
    lv_obj_set_pos(strip, x, y);
    lv_obj_set_size(strip, w, h);
    lv_obj_set_style_bg_color(strip, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(strip, lv_color_hex(0x404850), 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_radius(strip, Theme::S(3), 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    cell(strip, "FUEL QTY", &qty_);
    cell(strip, "FUEL FLOW", &flow_);
    cell(strip, "EGT", &egt_);

    chg_ = lv_label_create(strip);
    lv_obj_add_flag(chg_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_text_font(chg_, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(chg_, lv_color_black(), 0);
    lv_obj_set_style_bg_color(chg_, Theme::advisory(), 0);
    lv_obj_set_style_bg_opa(chg_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(chg_, Theme::S(2), 0);
    lv_label_set_text(chg_, "CHG");
    lv_obj_align(chg_, LV_ALIGN_TOP_RIGHT, -Theme::S(2), Theme::S(2));
    lv_obj_add_flag(chg_, LV_OBJ_FLAG_HIDDEN);
}

static void setIfChanged(lv_obj_t* l, char* cache, const char* text)
{
    if (strcmp(cache, text) != 0) {
        strncpy(cache, text, 15);
        cache[15] = '\0';
        lv_label_set_text(l, text);
    }
}

void EicasWidget::set(const PowerSnapshot& p)
{
    char b[16];
    if (!p.valid) {
        setIfChanged(qty_, last_[0], "--");
        setIfChanged(flow_, last_[1], "--");
        setIfChanged(egt_, last_[2], "--");
    } else {
        snprintf(b, sizeof b, "%u%%", p.pmu.battPct);
        setIfChanged(qty_, last_[0], b);
        if (p.pmu.charging) {
            snprintf(b, sizeof b, "REFUEL");
        } else if (std::isnan(p.fuelFlowPctPerH)) {
            snprintf(b, sizeof b, "--");
        } else {
            snprintf(b, sizeof b, "%.1f%%/h", p.fuelFlowPctPerH);
        }
        setIfChanged(flow_, last_[1], b);
        snprintf(b, sizeof b, "%d°C", int(std::lround(p.pmu.dieTempC10 / 10.0)));
        setIfChanged(egt_, last_[2], b);
    }
    int chg = (p.valid && p.pmu.charging) ? 1 : 0;
    if (chg != lastChg_) {
        lastChg_ = chg;
        if (chg) {
            lv_obj_remove_flag(chg_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(chg_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
