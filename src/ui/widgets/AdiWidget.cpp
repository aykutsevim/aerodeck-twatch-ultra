#include "ui/widgets/AdiWidget.h"
#include <cmath>
#include <cstdio>
#include "ui/Theme.h"

namespace {

constexpr float kD2R = 0.01745329252f;
constexpr float kVisibleDeg = 60.0f;    // vertical field of view of the instrument (±30°)
constexpr float kPitchClampDeg = 30.0f; // beyond this: full sky / full ground
constexpr float kMinDelta = 0.2f;       // invalidation threshold (degrees)

const lv_color_t kSky = LV_COLOR_MAKE(0x1E, 0x6F, 0xD9);
const lv_color_t kGround = LV_COLOR_MAKE(0x8B, 0x5A, 0x2B);
const lv_color_t kSymbol = LV_COLOR_MAKE(0xFF, 0xD0, 0x00);

struct V {
    float x, y;
};

// Clip polygon `in` (n vertices) against the half-plane (q - p)·nrm >= 0. Returns the vertex count.
int clipHalfPlane(const V* in, int n, V p, V nrm, V* out)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        V a = in[i], b = in[(i + 1) % n];
        float da = (a.x - p.x) * nrm.x + (a.y - p.y) * nrm.y;
        float db = (b.x - p.x) * nrm.x + (b.y - p.y) * nrm.y;
        if (da >= 0) {
            out[k++] = a;
        }
        if ((da >= 0) != (db >= 0)) {
            float t = da / (da - db);
            out[k++] = V{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
        }
    }
    return k;
}

void line(lv_layer_t* layer, V a, V b, lv_color_t c, int32_t w)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = lv_point_precise_t{a.x, a.y};
    d.p2 = lv_point_precise_t{b.x, b.y};
    d.color = c;
    d.width = w;
    d.opa = LV_OPA_COVER;
    lv_draw_line(layer, &d);
}

} // namespace

void AdiWidget::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t size)
{
    size_ = size;
    obj_ = lv_obj_create(parent);
    lv_obj_remove_style_all(obj_);
    lv_obj_set_pos(obj_, x, y);
    lv_obj_set_size(obj_, size, size);
    lv_obj_set_style_bg_color(obj_, kSky, 0);
    lv_obj_set_style_bg_opa(obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj_, Theme::S(6), 0);
    lv_obj_set_style_clip_corner(obj_, false, 0);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj_, onDraw, LV_EVENT_DRAW_MAIN, this);

    lock_ = lv_label_create(parent);
    lv_label_set_text(lock_, "LOCK");
    lv_obj_set_style_text_font(lock_, Theme::fontBody(), 0);
    lv_obj_set_style_text_color(lock_, Theme::hudGreen(), 0);
    lv_obj_set_style_bg_color(lock_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lock_, LV_OPA_60, 0);
    lv_obj_set_style_pad_hor(lock_, Theme::S(3), 0);
    lv_obj_align_to(lock_, obj_, LV_ALIGN_CENTER, 0, size / 6);
    lv_obj_add_flag(lock_, LV_OBJ_FLAG_HIDDEN);

    flag_ = lv_label_create(parent);
    lv_obj_set_style_text_font(flag_, Theme::fontTitle(), 0);
    lv_obj_set_style_bg_opa(flag_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(flag_, lv_color_black(), 0);
    lv_obj_set_style_pad_all(flag_, Theme::S(3), 0);
    lv_obj_set_style_border_width(flag_, 2, 0);
    lv_label_set_text(flag_, "ATT ALIGN");
    lv_obj_set_style_text_color(flag_, Theme::caution(), 0);
    lv_obj_set_style_border_color(flag_, Theme::caution(), 0);
    lv_obj_align_to(flag_, obj_, LV_ALIGN_CENTER, 0, -size / 5);
}

bool AdiWidget::set(const AttitudeSnapshot& a)
{
    sampleMs_ = a.sampleMs;
    bool statusChanged = first_ || a.status != status_;
    bool changed = statusChanged || a.level != level_ || std::fabs(a.pitchDeg - pitch_) >= kMinDelta ||
                   std::fabs(a.rollDeg - roll_) >= kMinDelta;
    if (!changed) {
        return false;
    }
    first_ = false;
    if (statusChanged) {
        if (a.status == AttStatus::Ok) {
            lv_obj_add_flag(flag_, LV_OBJ_FLAG_HIDDEN);
        } else {
            bool fail = a.status == AttStatus::Fail;
            lv_label_set_text(flag_, fail ? "ATT FAIL" : "ATT ALIGN");
            lv_color_t c = fail ? Theme::warning() : Theme::caution();
            lv_obj_set_style_text_color(flag_, c, 0);
            lv_obj_set_style_border_color(flag_, c, 0);
            lv_obj_remove_flag(flag_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align_to(flag_, obj_, LV_ALIGN_CENTER, 0, -size_ / 5);
        }
    }
    if (a.level != level_) {
        if (a.level) {
            lv_obj_remove_flag(lock_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lock_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    status_ = a.status;
    level_ = a.level && a.status == AttStatus::Ok;
    // A failed/aligning reference shows a fixed level picture (FR-005).
    pitch_ = status_ == AttStatus::Ok ? a.pitchDeg : 0.0f;
    roll_ = status_ == AttStatus::Ok ? a.rollDeg : 0.0f;
    lv_obj_invalidate(obj_);
    return true;
}

void AdiWidget::onDraw(lv_event_t* e)
{
    auto* self = static_cast<AdiWidget*>(lv_event_get_user_data(e));
    self->draw(lv_event_get_layer(e));
}

void AdiWidget::draw(lv_layer_t* layer)
{
    drawnSampleMs_ = sampleMs_;
    lv_area_t a;
    lv_obj_get_coords(obj_, &a);
    const float s = float(size_);
    const float ppd = s / kVisibleDeg;   // pixels per degree
    const V c{(a.x1 + a.x2 + 1) * 0.5f, (a.y1 + a.y2 + 1) * 0.5f};

    // Horizon frame: u along the horizon, n towards the ground. Roll right → horizon tilts left (φ = -roll).
    const float phi = -roll_ * kD2R;
    const V u{std::cos(phi), std::sin(phi)};
    const V n{-u.y, u.x};
    float pitch = pitch_;
    if (pitch > kPitchClampDeg + 2) pitch = kPitchClampDeg + 2;     // just past the edge: full ground/sky
    if (pitch < -kPitchClampDeg - 2) pitch = -kPitchClampDeg - 2;
    const V p{c.x + n.x * pitch * ppd, c.y + n.y * pitch * ppd};

    // Ground = square ∩ half-plane on the n side of the horizon line.
    const V sq[4] = {{float(a.x1), float(a.y1)}, {float(a.x2 + 1), float(a.y1)},
                     {float(a.x2 + 1), float(a.y2 + 1)}, {float(a.x1), float(a.y2 + 1)}};
    V poly[8];
    int k = clipHalfPlane(sq, 4, p, n, poly);
    lv_draw_triangle_dsc_t td;
    lv_draw_triangle_dsc_init(&td);
    td.color = kGround;
    td.opa = LV_OPA_COVER;
    for (int i = 1; i + 1 < k; i++) {   // triangle fan
        td.p[0] = lv_point_precise_t{poly[0].x, poly[0].y};
        td.p[1] = lv_point_precise_t{poly[i].x, poly[i].y};
        td.p[2] = lv_point_precise_t{poly[i + 1].x, poly[i + 1].y};
        lv_draw_triangle(layer, &td);
    }

    const int32_t lw = Theme::S(1) < 2 ? 2 : Theme::S(1);
    // Horizon line
    line(layer, V{p.x - u.x * s, p.y - u.y * s}, V{p.x + u.x * s, p.y + u.y * s}, lv_color_white(), lw);

    // Pitch ladder: every 5° within ±20°, numbers at 10° (trimmed from ±25° for FPS, ADR-011).
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    ld.font = Theme::fontSmall();
    ld.color = lv_color_white();
    for (int deg = -20; deg <= 20; deg += 5) {
        if (deg == 0) continue;
        const float off = (pitch_ - deg) * ppd;   // a +10° mark sits 10° above the horizon
        const V m{c.x + n.x * off, c.y + n.y * off};
        const float half = (deg % 10 == 0 ? 0.18f : 0.09f) * s;
        line(layer, V{m.x - u.x * half, m.y - u.y * half}, V{m.x + u.x * half, m.y + u.y * half}, lv_color_white(), lw);
        if (deg % 10 == 0) {
            char txt[4];
            snprintf(txt, sizeof txt, "%d", deg < 0 ? -deg : deg);
            ld.text = txt;
            const V t{m.x + u.x * (half + 0.07f * s), m.y + u.y * (half + 0.07f * s)};
            lv_area_t ta{int32_t(t.x - 0.05f * s), int32_t(t.y - 0.04f * s), int32_t(t.x + 0.05f * s),
                         int32_t(t.y + 0.04f * s)};
            lv_draw_label(layer, &ld, &ta);
        }
    }

    // Fixed aircraft symbol (yellow): two wing bars with down-ticks and a centre dot.
    const int32_t sw = lw + 2;
    const float wing = 0.30f * s, gap = 0.08f * s, tick = 0.05f * s;
    line(layer, V{c.x - wing, c.y}, V{c.x - gap, c.y}, kSymbol, sw);
    line(layer, V{c.x - gap, c.y}, V{c.x - gap, c.y + tick}, kSymbol, sw);
    line(layer, V{c.x + gap, c.y}, V{c.x + wing, c.y}, kSymbol, sw);
    line(layer, V{c.x + gap, c.y}, V{c.x + gap, c.y + tick}, kSymbol, sw);
    line(layer, V{c.x - 2, c.y}, V{c.x + 2, c.y}, kSymbol, sw + 2);

    // Roll index: fixed pointer at the top centre.
    lv_draw_triangle_dsc_t ri;
    lv_draw_triangle_dsc_init(&ri);
    ri.color = kSymbol;
    ri.opa = LV_OPA_COVER;
    const float ty = a.y1 + 0.03f * s, th = 0.05f * s;
    ri.p[0] = lv_point_precise_t{c.x, ty + th};
    ri.p[1] = lv_point_precise_t{c.x - th * 0.7f, ty};
    ri.p[2] = lv_point_precise_t{c.x + th * 0.7f, ty};
    lv_draw_triangle(layer, &ri);
}
