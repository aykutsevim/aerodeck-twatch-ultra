#include "ui/PageIndicator.h"
#include <lvgl.h>
#include "ui/Theme.h"

namespace PageIndicator {

static constexpr int kMaxDots = 8;
static lv_obj_t* s_box;
static lv_obj_t* s_title;
static lv_obj_t* s_dots[kMaxDots];
static lv_timer_t* s_hide;

static void hideCb(lv_timer_t*)
{
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}

void begin()
{
    s_box = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_box);
    lv_obj_set_size(s_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_70, 0);
    lv_obj_set_style_radius(s_box, Theme::S(10), 0);
    lv_obj_set_style_pad_all(s_box, Theme::S(6), 0);
    lv_obj_set_style_pad_row(s_box, Theme::S(4), 0);
    lv_obj_set_flex_flow(s_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(s_box, LV_ALIGN_BOTTOM_MID, 0, -Theme::S(8));
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_CLICKABLE);

    s_title = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_title, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(s_title, Theme::fg(), 0);

    lv_obj_t* row = lv_obj_create(s_box);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, Theme::S(6), 0);
    for (int i = 0; i < kMaxDots; i++) {
        lv_obj_t* d = lv_obj_create(row);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, Theme::S(8), Theme::S(8));
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 1, 0);
        lv_obj_set_style_border_color(d, Theme::hudGreen(), 0);
        lv_obj_set_style_bg_color(d, Theme::hudGreen(), 0);
        s_dots[i] = d;
    }
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    s_hide = lv_timer_create(hideCb, 1000, nullptr);
    lv_timer_set_auto_delete(s_hide, false);   // keep the one-shot timer alive for reuse
    lv_timer_pause(s_hide);
}

void show(int index, int count, const char* title)
{
    lv_label_set_text(s_title, title ? title : "");
    for (int i = 0; i < kMaxDots; i++) {
        if (i < count) {
            lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(s_dots[i], i == index ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        } else {
            lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(s_hide);
    lv_timer_set_repeat_count(s_hide, 1);
    lv_timer_resume(s_hide);
}

} // namespace PageIndicator
