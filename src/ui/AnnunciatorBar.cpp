#include "ui/AnnunciatorBar.h"
#include <Arduino.h>
#include <lvgl.h>
#include "core/NvgStyle.h"
#include "services/AlertManager.h"
#include "services/HapticService.h"
#include "ui/Theme.h"

namespace AnnunciatorBar {

namespace {

lv_obj_t* s_bar;
lv_obj_t* s_label;
uint32_t s_seq = UINT32_MAX;
AlertSnapshot s_snap;     // last read (UI task only)
size_t s_index;           // cycling position when all are acknowledged
bool s_flashOn = true;
bool s_nvg;               // S7: NVG looks (core::NvgStyle)

void colours(core::AlertLevel l, lv_color_t& bg, lv_color_t& fg)
{
    switch (l) {
    case core::AlertLevel::Warning: bg = Theme::warning(); fg = lv_color_white(); break;
    case core::AlertLevel::Caution: bg = Theme::caution(); fg = lv_color_black(); break;
    default: bg = Theme::advisory(); fg = lv_color_black(); break;
    }
}

void render()
{
    if (s_snap.total == 0 || s_snap.n == 0) {
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    // Unacknowledged: show the master. Acknowledged: cycle through all (highest first).
    if (s_snap.anyUnacked || s_index >= s_snap.n) {
        s_index = 0;
    }
    const core::Alert& a = s_snap.items[s_index];
    lv_color_t bg, fg;
    colours(a.level, bg, fg);
    // S7 (FR-015): in NVG the level is told by shape and motion, not colour (the flush filter makes it green).
    const core::NvgStyle::AlertLook look = core::NvgStyle::forLevel(a.level, s_nvg, millis());
    int32_t border = 0;
    if (s_nvg) {
        if (look.inverse) {
            bg = Theme::hudGreen();
            fg = lv_color_black();
        } else {   // outline
            bg = lv_color_black();
            fg = Theme::hudGreen();
            border = Theme::S(2);
        }
        if (!look.visible) {   // WARNING blink off-phase: dark band
            bg = lv_color_black();
            fg = lv_color_black();
        }
    }
    lv_obj_set_style_border_width(s_bar, border, 0);
    lv_obj_set_style_border_color(s_bar, Theme::hudGreen(), 0);
    lv_obj_set_style_bg_color(s_bar, bg, 0);
    lv_obj_set_style_text_color(s_label, fg, 0);
    if (s_snap.anyUnacked) {
        if (s_snap.total > 1) {
            lv_label_set_text_fmt(s_label, "%s  +%u", a.text, unsigned(s_snap.total - 1));
        } else {
            lv_label_set_text(s_label, a.text);
        }
        lv_obj_set_style_bg_opa(s_bar, s_flashOn ? LV_OPA_COVER : LV_OPA_40, 0);
    } else {
        if (s_snap.total > 1) {
            lv_label_set_text_fmt(s_label, "%s  %u/%u", a.text, unsigned(s_index + 1), unsigned(s_snap.total));
        } else {
            lv_label_set_text(s_label, a.text);
        }
        lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    }
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
}

void flashCb(lv_timer_t*)   // 500 ms: 1 Hz flash while unacknowledged
{
    if (s_snap.total && s_snap.anyUnacked) {
        s_flashOn = !s_flashOn;
        lv_obj_set_style_bg_opa(s_bar, s_flashOn ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

void blinkCb(lv_timer_t*)   // 250 ms: S7 NVG WARNING blink (2 Hz)
{
    if (s_nvg && s_snap.total && s_snap.n && s_snap.items[s_index < s_snap.n ? s_index : 0].level ==
                                                  core::AlertLevel::Warning) {
        render();
    }
}

void cycleCb(lv_timer_t*)   // 3 s: cycle when all are acknowledged
{
    if (s_snap.total > 1 && !s_snap.anyUnacked) {
        s_index = (s_index + 1) % s_snap.n;
        render();
    }
}

void tapped(lv_event_t*)
{
    HapticService::click();
    AlertManager::ackAll();   // FR-007
    update();
}

} // namespace

void begin()
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_bar);
    lv_obj_add_style(s_bar, Theme::banner(), 0);
    lv_obj_set_size(s_bar, lv_pct(100), Theme::S(28));   // drop-in for the S0 banner (PfdLayout bannerH)
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    // The tap is consumed: clickable, no gesture bubbling (never a page swipe or a control press below).
    lv_obj_add_flag(s_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_bar, tapped, LV_EVENT_CLICKED, nullptr);
    s_label = lv_label_create(s_bar);
    // Rounded corners (r = 64): at the band's vertical centre the corner cuts ~14 px; keep the text inside.
    lv_obj_set_width(s_label, Theme::width() - 2 * Theme::S(24));
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_label);
    lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(flashCb, 500, nullptr);
    lv_timer_create(cycleCb, 3000, nullptr);
    lv_timer_create(blinkCb, core::NvgStyle::kBlinkHalfMs, nullptr);
    update();
}

void setNvg(bool on)
{
    s_nvg = on;
    if (s_bar) {
        render();
    }
}

void setTopOffset(int32_t px)
{
    if (s_bar) {
        lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, px);
    }
}

void update()
{
    if (!s_bar) {
        return;
    }
    const uint32_t seq = AlertManager::status().seq();
    if (seq == s_seq) {
        return;
    }
    s_seq = seq;
    s_snap = AlertManager::status().read();
    s_flashOn = true;
    render();
}

} // namespace AnnunciatorBar
