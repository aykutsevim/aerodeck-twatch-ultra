#include "ui/widgets/TapeWidget.h"
#include <cmath>
#include <cstdio>
#include "ui/Theme.h"
#include "ui/fonts/fonts.h"

namespace {
const lv_color_t kTapeBg = LV_COLOR_MAKE(0x20, 0x24, 0x2A);
constexpr float kSpanDeg = 60.0f;   // heading tape shows ±30° around the current value
}

void TapeWidget::create(lv_obj_t* parent, Dir dir, int32_t x, int32_t y, int32_t w, int32_t h, int modulo,
                        const char* caption)
{
    dir_ = dir;
    modulo_ = modulo;
    obj_ = lv_obj_create(parent);
    lv_obj_remove_style_all(obj_);
    lv_obj_set_pos(obj_, x, y);
    lv_obj_set_size(obj_, w, h);
    lv_obj_set_style_bg_color(obj_, kTapeBg, 0);
    lv_obj_set_style_bg_opa(obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj_, Theme::S(3), 0);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);

    if (dir == Dir::Horizontal) {
        lv_obj_add_event_cb(obj_, onDraw, LV_EVENT_DRAW_MAIN, this);
        return;
    }

    // Vertical tape: caption at the top, neighbours above/below, value box in the middle.
    lv_obj_t* cap = lv_label_create(obj_);
    lv_obj_set_style_text_font(cap, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(cap, Theme::dim(), 0);
    lv_label_set_text(cap, caption ? caption : "");
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, Theme::S(2));

    const int32_t step = h / 6;
    const int32_t offs[4] = {-2 * step, -step, step, 2 * step};
    for (int i = 0; i < 4; i++) {
        neigh_[i] = lv_label_create(obj_);
        lv_obj_set_style_text_font(neigh_[i], Theme::fontBody(), 0);
        lv_obj_set_style_text_color(neigh_[i], lv_color_hex(0xB0B0B0), 0);
        lv_obj_align(neigh_[i], LV_ALIGN_CENTER, 0, offs[i]);
    }
    box_ = lv_obj_create(obj_);
    lv_obj_remove_style_all(box_);
    lv_obj_set_size(box_, w - Theme::S(4), Theme::S(30));
    lv_obj_set_style_bg_color(box_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box_, Theme::hudGreen(), 0);
    lv_obj_set_style_border_width(box_, 2, 0);
    lv_obj_set_style_radius(box_, Theme::S(3), 0);
    lv_obj_center(box_);
    boxLbl_ = lv_label_create(box_);
    lv_obj_set_style_text_font(boxLbl_, &dseg7_40, 0);
    lv_obj_set_style_text_color(boxLbl_, Theme::hudGreen(), 0);
    lv_obj_center(boxLbl_);
    setValue(-1);
}

void TapeWidget::setValue(int v)
{
    if (v == value_) {
        return;
    }
    value_ = v;
    if (v < 0) {
        lv_label_set_text(boxLbl_, "--");
        for (auto* n : neigh_) {
            lv_label_set_text(n, "");
        }
        return;
    }
    lv_label_set_text_fmt(boxLbl_, "%02d", v);
    const int d[4] = {2, 1, -1, -2};
    for (int i = 0; i < 4; i++) {
        int n = ((v + d[i]) % modulo_ + modulo_) % modulo_;
        lv_label_set_text_fmt(neigh_[i], "%02d", n);
    }
}

void TapeWidget::setHeading(float deg)
{
    if (deg < 0 && heading_ < 0) {
        return;
    }
    // 1° steps (≈6 redraws/s) instead of every frame: keeps the horizon at ≥30 FPS (ADR-011).
    if (deg >= 0) {
        deg = std::floor(deg);
        if (deg == heading_) {
            return;
        }
    }
    heading_ = deg;
    lv_obj_invalidate(obj_);
}

void TapeWidget::onDraw(lv_event_t* e)
{
    auto* self = static_cast<TapeWidget*>(lv_event_get_user_data(e));
    self->drawHeading(lv_event_get_layer(e));
}

void TapeWidget::drawHeading(lv_layer_t* layer)
{
    lv_area_t a;
    lv_obj_get_coords(obj_, &a);
    const float w = float(lv_area_get_width(&a));
    const float h = float(lv_area_get_height(&a));
    const float cx = a.x1 + w * 0.5f;
    const float ppd = w / kSpanDeg;
    const float hd = heading_ < 0 ? 0.0f : heading_;

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_white();
    ld.width = 2;
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.font = Theme::fontSmall();
    td.color = lv_color_white();
    td.align = LV_TEXT_ALIGN_CENTER;

    // Ticks every 5°, numbers every 30° (compass style 00, 03, 06 … 33).
    const int first = int(std::floor((hd - kSpanDeg / 2) / 5.0f)) * 5;
    for (int d = first; d <= hd + kSpanDeg / 2 + 5; d += 5) {
        const float x = cx + (d - hd) * ppd;
        const bool major = (d % 30 + 30) % 30 == 0;
        const float len = major ? h * 0.30f : h * 0.15f;
        ld.p1 = lv_point_precise_t{x, float(a.y2 + 1) - len};
        ld.p2 = lv_point_precise_t{x, float(a.y2 + 1)};
        lv_draw_line(layer, &ld);
        if (major) {
            char txt[4];
            int norm = ((d % 360) + 360) % 360;
            snprintf(txt, sizeof txt, "%02d", norm / 10);
            td.text = txt;
            lv_area_t ta{int32_t(x - 20), int32_t(a.y2 + 1 - len - Theme::S(14)), int32_t(x + 20),
                         int32_t(a.y2 + 1 - len)};
            lv_draw_label(layer, &td, &ta);
        }
    }

    // Value box in the centre with the 3-digit heading (seconds × 6).
    const int32_t bw = Theme::S(40), bh = int32_t(h * 0.55f);
    lv_area_t box{int32_t(cx - bw / 2), a.y1, int32_t(cx + bw / 2), a.y1 + bh};
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_black();
    rd.bg_opa = LV_OPA_COVER;
    rd.border_color = Theme::hudGreen();
    rd.border_width = 2;
    rd.radius = Theme::S(2);
    lv_draw_rect(layer, &rd, &box);
    char val[5];
    if (heading_ < 0) {
        snprintf(val, sizeof val, "---");
    } else {
        snprintf(val, sizeof val, "%03d", int(heading_) % 360);
    }
    td.text = val;
    td.font = Theme::fontBody();
    td.color = Theme::hudGreen();
    const int32_t fh = lv_font_get_line_height(Theme::fontBody());
    lv_area_t vt{box.x1, box.y1 + (bh - fh) / 2, box.x2, box.y2};
    lv_draw_label(layer, &td, &vt);
}
