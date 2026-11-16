#include "ui/widgets/TankWidget.h"
#include "ui/Theme.h"

void TankWidget::create(lv_obj_t* parent, int32_t w, int32_t h)
{
    obj_ = lv_obj_create(parent);
    lv_obj_remove_style_all(obj_);
    lv_obj_set_size(obj_, w, h);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj_, onDraw, LV_EVENT_DRAW_MAIN, this);
}

void TankWidget::set(float fraction)
{
    if (fraction < 0.0f) {
        fraction = 0.0f;
    }
    if (fraction > 1.0f) {
        fraction = 1.0f;
    }
    const int pct = int(fraction * 100.0f + 0.5f);
    if (pct != pct_) {
        pct_ = pct;
        lv_obj_invalidate(obj_);
    }
}

// Layout inside the widget box (x right, y down):
//   [nose cone][====== body (capsule) ======][fins]
void TankWidget::onDraw(lv_event_t* e)
{
    auto* self = static_cast<TankWidget*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(self->obj_, &a);
    const int32_t W = lv_area_get_width(&a), H = lv_area_get_height(&a);
    const int32_t finW = W / 9, noseW = W / 7;
    const int32_t bodyH = H * 6 / 10;
    const int32_t bx1 = a.x1 + noseW, bx2 = a.x2 - finW;
    const int32_t by1 = a.y1 + (H - bodyH) / 2, by2 = by1 + bodyH - 1;
    const int32_t line = Theme::S(2) < 2 ? 2 : Theme::S(2);
    const int pct = self->pct_ < 0 ? 100 : self->pct_;
    const lv_color_t fuel = pct == 0 ? Theme::warning() : pct < 10 ? Theme::caution() : Theme::hudGreen();

    // Fuel: a level across nose + body, clipped to the tank silhouette.
    const int32_t levelY = by2 - (bodyH * pct) / 100;
    lv_draw_rect_dsc_t fr;
    lv_draw_rect_dsc_init(&fr);
    fr.bg_color = fuel;
    fr.bg_opa = LV_OPA_80;
    if (pct > 0) {
        // A rounded slab from the level down; when full it is exactly the capsule body.
        const int32_t inset = line + 1;
        lv_area_t fuelA = {bx1 - noseW / 2 + inset, levelY + inset, bx2 - inset, by2 - inset};
        if (fuelA.y2 > fuelA.y1) {
            const int32_t hh = fuelA.y2 - fuelA.y1;
            fr.radius = hh / 2 < bodyH / 2 ? hh / 2 : bodyH / 2;
            lv_draw_rect(layer, &fr, &fuelA);
        }
    }

    // Outline: capsule body + nose cone + two tail fins.
    lv_draw_rect_dsc_t ol;
    lv_draw_rect_dsc_init(&ol);
    ol.bg_opa = LV_OPA_TRANSP;
    ol.border_color = Theme::fg();
    ol.border_width = line;
    ol.radius = bodyH / 2;
    lv_area_t body = {bx1 - noseW / 2, by1, bx2, by2};
    lv_draw_rect(layer, &ol, &body);

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = Theme::fg();
    ld.width = line;
    const int32_t cy = (by1 + by2) / 2;
    // nose cone tip
    ld.p1 = lv_point_precise_t{lv_value_precise_t(bx1 - noseW / 2 + bodyH / 4), lv_value_precise_t(by1 + bodyH / 6)};
    ld.p2 = lv_point_precise_t{lv_value_precise_t(a.x1), lv_value_precise_t(cy)};
    lv_draw_line(layer, &ld);
    ld.p1 = lv_point_precise_t{lv_value_precise_t(bx1 - noseW / 2 + bodyH / 4), lv_value_precise_t(by2 - bodyH / 6)};
    lv_draw_line(layer, &ld);
    // fins
    ld.p1 = lv_point_precise_t{lv_value_precise_t(bx2 - finW), lv_value_precise_t(by1)};
    ld.p2 = lv_point_precise_t{lv_value_precise_t(a.x2), lv_value_precise_t(a.y1 + line)};
    lv_draw_line(layer, &ld);
    ld.p1 = lv_point_precise_t{lv_value_precise_t(bx2 - finW), lv_value_precise_t(by2)};
    ld.p2 = lv_point_precise_t{lv_value_precise_t(a.x2), lv_value_precise_t(a.y2 - line)};
    lv_draw_line(layer, &ld);
    ld.p1 = lv_point_precise_t{lv_value_precise_t(a.x2), lv_value_precise_t(a.y1 + line)};
    ld.p2 = lv_point_precise_t{lv_value_precise_t(a.x2), lv_value_precise_t(a.y2 - line)};
    lv_draw_line(layer, &ld);
}
