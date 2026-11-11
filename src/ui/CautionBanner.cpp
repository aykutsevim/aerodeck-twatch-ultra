#include "ui/CautionBanner.h"
#include <lvgl.h>
#include "services/Cautions.h"
#include "ui/Theme.h"

namespace CautionBanner {

static lv_obj_t* s_bar;
static lv_obj_t* s_label;
static size_t s_index;

static void show()
{
    Caution items[Cautions::kMax];
    size_t n = Cautions::active(items, Cautions::kMax);
    if (n == 0) {
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (s_index >= n) {
        s_index = 0;
    }
    if (n > 1) {
        lv_label_set_text_fmt(s_label, "%s  (%u/%u)", items[s_index].text, unsigned(s_index + 1), unsigned(n));
    } else {
        lv_label_set_text(s_label, items[0].text);
    }
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
}

static void cycleCb(lv_timer_t*)
{
    s_index++;
    show();
}

void begin()
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_bar);
    lv_obj_add_style(s_bar, Theme::banner(), 0);
    lv_obj_set_size(s_bar, lv_pct(100), Theme::S(28));
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_CLICKABLE);
    s_label = lv_label_create(s_bar);
    lv_obj_center(s_label);
    lv_timer_create(cycleCb, 3000, nullptr);
    show();
}

void refresh()
{
    show();
}

} // namespace CautionBanner
