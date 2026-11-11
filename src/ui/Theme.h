// AeroDeck theme: resolution-independent sizing S(px) and palette tokens (FR-025, SAS §8).
#pragma once
#include <lvgl.h>
#include "core/Metar.h"
#include "core/Types.h"

namespace Theme {

void begin(const core::DisplayInfo& d);
float scale();
inline int32_t S(int32_t px) { return int32_t(px * scale() + 0.5f); }
int32_t width();
int32_t height();
int32_t cornerRadius();   // screen corner radius in px (0 = square corners)

// Palette tokens
lv_color_t bg();
lv_color_t fg();
lv_color_t dim();
lv_color_t hudGreen();
lv_color_t caution();
lv_color_t warning();
lv_color_t advisory();
lv_color_t accent();
// Flight category colours (S3, FR-007). NVG variants come with a later slice.
lv_color_t catVfr();
lv_color_t catMvfr();
lv_color_t catIfr();
lv_color_t catLifr();
lv_color_t catUnknown();
lv_color_t catColor(core::Metar::Category c);

// Fonts picked by scale
const lv_font_t* fontSmall();
const lv_font_t* fontBody();
const lv_font_t* fontTitle();
const lv_font_t* fontHuge();

// Shared styles (initialised once by begin)
lv_style_t* screen();
lv_style_t* title();
lv_style_t* body();
lv_style_t* button();
lv_style_t* banner();

// Apply the screen style to a freshly created screen object.
void styleScreen(lv_obj_t* scr);

} // namespace Theme
