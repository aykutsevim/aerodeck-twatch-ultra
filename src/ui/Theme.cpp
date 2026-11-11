#include "ui/Theme.h"

namespace Theme {

static float s_scale = 1.0f;
static int32_t s_w = 240, s_h = 240;
static int32_t s_cornerR = 0;
static lv_style_t s_screen, s_title, s_body, s_button, s_banner;
static const lv_font_t *s_small, *s_bodyFont, *s_titleFont, *s_huge;

static const lv_font_t* pick(int32_t px)
{
    // Montserrat sizes enabled in LilyGoLib's lv_conf.h: 10..28 (even), 32, 36, 40, 48.
    if (px >= 46) return &lv_font_montserrat_48;
    if (px >= 39) return &lv_font_montserrat_40;
    if (px >= 35) return &lv_font_montserrat_36;
    if (px >= 31) return &lv_font_montserrat_32;
    if (px >= 27) return &lv_font_montserrat_28;
    if (px >= 23) return &lv_font_montserrat_24;
    if (px >= 19) return &lv_font_montserrat_20;
    if (px >= 15) return &lv_font_montserrat_16;
    return &lv_font_montserrat_14;
}

void begin(const core::DisplayInfo& d)
{
    s_w = d.w;
    s_h = d.h;
    s_cornerR = d.cornerR;
    int32_t m = d.w < d.h ? d.w : d.h;
    s_scale = m / 240.0f;

    s_small = pick(S(10));
    s_bodyFont = pick(S(14));
    s_titleFont = pick(S(20));
    s_huge = pick(S(28));

    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, bg());
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_text_color(&s_screen, fg());
    lv_style_set_text_font(&s_screen, s_bodyFont);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_border_width(&s_screen, 0);

    lv_style_init(&s_title);
    lv_style_set_text_font(&s_title, s_titleFont);
    lv_style_set_text_color(&s_title, hudGreen());

    lv_style_init(&s_body);
    lv_style_set_text_font(&s_body, s_bodyFont);
    lv_style_set_text_color(&s_body, fg());

    lv_style_init(&s_button);
    lv_style_set_min_height(&s_button, S(48));   // NFR-USE-001 touch target
    lv_style_set_radius(&s_button, S(8));
    lv_style_set_bg_color(&s_button, lv_color_hex(0x203040));
    lv_style_set_text_color(&s_button, fg());
    lv_style_set_text_font(&s_button, s_bodyFont);

    lv_style_init(&s_banner);
    lv_style_set_bg_color(&s_banner, caution());
    lv_style_set_bg_opa(&s_banner, LV_OPA_COVER);
    lv_style_set_text_color(&s_banner, lv_color_black());
    lv_style_set_text_font(&s_banner, s_bodyFont);
    lv_style_set_radius(&s_banner, 0);
    lv_style_set_border_width(&s_banner, 0);
    lv_style_set_pad_ver(&s_banner, S(2));
}

float scale() { return s_scale; }
int32_t width() { return s_w; }
int32_t height() { return s_h; }
int32_t cornerRadius() { return s_cornerR; }

lv_color_t bg() { return lv_color_black(); }
lv_color_t fg() { return lv_color_hex(0xE8E8E8); }
lv_color_t dim() { return lv_color_hex(0x808080); }
lv_color_t hudGreen() { return lv_color_hex(0x00FF66); }
lv_color_t caution() { return lv_color_hex(0xFFB000); }
lv_color_t warning() { return lv_color_hex(0xFF3030); }
lv_color_t advisory() { return lv_color_hex(0x00C8FF); }
lv_color_t accent() { return lv_color_hex(0x3A7BD5); }
lv_color_t catVfr() { return lv_color_hex(0x00C000); }
lv_color_t catMvfr() { return lv_color_hex(0x3A7BFF); }
lv_color_t catIfr() { return lv_color_hex(0xFF3030); }
lv_color_t catLifr() { return lv_color_hex(0xFF40FF); }
lv_color_t catUnknown() { return dim(); }
lv_color_t catColor(core::Metar::Category c)
{
    switch (c) {
    case core::Metar::Category::Vfr: return catVfr();
    case core::Metar::Category::Mvfr: return catMvfr();
    case core::Metar::Category::Ifr: return catIfr();
    case core::Metar::Category::Lifr: return catLifr();
    default: return catUnknown();
    }
}

const lv_font_t* fontSmall() { return s_small; }
const lv_font_t* fontBody() { return s_bodyFont; }
const lv_font_t* fontTitle() { return s_titleFont; }
const lv_font_t* fontHuge() { return s_huge; }

lv_style_t* screen() { return &s_screen; }
lv_style_t* title() { return &s_title; }
lv_style_t* body() { return &s_body; }
lv_style_t* button() { return &s_button; }
lv_style_t* banner() { return &s_banner; }

void styleScreen(lv_obj_t* scr)
{
    lv_obj_add_style(scr, &s_screen, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace Theme
