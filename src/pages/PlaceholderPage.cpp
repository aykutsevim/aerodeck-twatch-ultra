#include "pages/PlaceholderPage.h"
#include "ui/Theme.h"

void PlaceholderPage::onCreate(lv_obj_t* scr)
{
    lv_obj_t* t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, Theme::fontHuge(), 0);
    lv_obj_set_style_text_color(t, Theme::advisory(), 0);
    lv_label_set_text(t, title_);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -Theme::S(10));

    lv_obj_t* s = lv_label_create(scr);
    lv_obj_set_style_text_font(s, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(s, Theme::dim(), 0);
    lv_label_set_text(s, "Coming in a later slice");
    lv_obj_align(s, LV_ALIGN_CENTER, 0, Theme::S(20));
}
