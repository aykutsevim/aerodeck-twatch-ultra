#include "pages/KaanPage.h"
#include "assets/KaanArt.h"
#include "ui/Theme.h"

void KaanPage::onCreate(lv_obj_t* scr)
{
    // The same A8 art as the boot splash (fits the rounded screen), tinted with the foreground colour on black.
    dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc_.header.cf = LV_COLOR_FORMAT_A8;
    dsc_.header.w = kKaanSplash.w;
    dsc_.header.h = kKaanSplash.h;
    dsc_.header.stride = kKaanSplash.w;
    dsc_.data = kKaanSplash.a8;
    dsc_.data_size = uint32_t(kKaanSplash.w) * kKaanSplash.h;
    lv_obj_t* img = lv_image_create(scr);
    lv_image_set_src(img, &dsc_);
    lv_obj_center(img);
    lv_obj_set_style_image_recolor(img, Theme::fg(), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);   // swipes go to the screen (page navigation)
}
