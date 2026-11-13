#include "ui/widgets/EjectHandle.h"
#include <Arduino.h>
#include "services/EventBus.h"
#include "services/HapticService.h"
#include "services/Log.h"
#include "ui/Theme.h"

using core::EjectGesture;
using S = EjectGesture::State;

namespace {
const lv_color_t kYellow = lv_color_hex(0xFFD000);
const lv_color_t kBlack = lv_color_black();
}

void EjectHandle::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t travel)
{
    EjectGesture::Params p;
    p.travelPx = travel;
    g_ = EjectGesture(p);
    y0_ = y;

    obj_ = lv_obj_create(parent);
    lv_obj_remove_style_all(obj_);
    lv_obj_set_pos(obj_, x, y);
    lv_obj_set_size(obj_, w, h);
    lv_obj_set_style_bg_color(obj_, kYellow, 0);
    lv_obj_set_style_bg_opa(obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj_, kYellow, 0);
    lv_obj_set_style_border_width(obj_, 0, 0);
    lv_obj_add_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    // Drags on the handle never change pages or scroll (research R8).
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj_, onDraw, LV_EVENT_DRAW_MAIN_END, this);
    lv_obj_add_event_cb(obj_, onEvent, LV_EVENT_ALL, this);

    // Readable plate in the middle of the stripes.
    plate_ = lv_obj_create(obj_);
    lv_obj_remove_style_all(plate_);
    lv_obj_set_style_bg_color(plate_, kYellow, 0);
    lv_obj_set_style_bg_opa(plate_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(plate_, Theme::S(4), 0);
    lv_obj_set_style_pad_hor(plate_, Theme::S(6), 0);
    lv_obj_set_style_pad_ver(plate_, Theme::S(1), 0);
    lv_obj_set_size(plate_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(plate_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(plate_);
    label_ = lv_label_create(plate_);
    lv_obj_set_style_text_color(label_, kBlack, 0);
    lv_obj_set_style_text_font(label_, Theme::fontBody(), 0);
    lv_label_set_text(label_, "PULL TO EJECT");

    lv_timer_create(onTimer, 100, this);
}

void EjectHandle::onDraw(lv_event_t* e)
{
    auto* self = static_cast<EjectHandle*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(self->obj_, &a);
    const int32_t h = lv_area_get_height(&a);
    const int32_t sw = Theme::S(12);   // stripe width
    lv_draw_triangle_dsc_t td;
    lv_draw_triangle_dsc_init(&td);
    td.color = kBlack;
    td.opa = LV_OPA_COVER;
    // Black 45° parallelograms every 2 × sw, clipped to the handle by the layer.
    for (int32_t x = a.x1 - h; x < a.x2 + 1; x += 2 * sw) {
        const float x0 = float(x), y1 = float(a.y1), y2 = float(a.y2 + 1);
        td.p[0] = lv_point_precise_t{x0, y2};
        td.p[1] = lv_point_precise_t{x0 + h, y1};
        td.p[2] = lv_point_precise_t{x0 + h + sw, y1};
        lv_draw_triangle(layer, &td);
        td.p[0] = lv_point_precise_t{x0, y2};
        td.p[1] = lv_point_precise_t{x0 + h + sw, y1};
        td.p[2] = lv_point_precise_t{x0 + sw, y2};
        lv_draw_triangle(layer, &td);
    }
}

void EjectHandle::onEvent(lv_event_t* e)
{
    auto* self = static_cast<EjectHandle*>(lv_event_get_user_data(e));
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* in = lv_indev_active();
    if (!in) {
        return;
    }
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    const uint32_t now = millis();
    switch (code) {
    case LV_EVENT_PRESSED: self->g_.press(pt.x, pt.y, now); break;
    case LV_EVENT_PRESSING: self->g_.move(pt.x, pt.y, now); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: self->g_.release(pt.x, pt.y, now); break;
    default: return;
    }
    self->sync(now);
}

void EjectHandle::onTimer(lv_timer_t* t)
{
    auto* self = static_cast<EjectHandle*>(lv_timer_get_user_data(t));
    const uint32_t now = millis();
    self->g_.tick(now);
    self->flash_ = !self->flash_;
    self->sync(now);
    if (self->g_.state() == S::Armed) {
        lv_obj_set_style_border_width(self->obj_, self->flash_ ? Theme::S(3) : 0, 0);
        lv_obj_set_style_border_color(self->obj_, lv_color_white(), 0);
    }
}

void EjectHandle::sync(uint32_t now)
{
    const EjectGesture::Cancel c = g_.takeCancel();
    if (c != EjectGesture::Cancel::None && shown_ != S::Idle && shown_ != S::FirstTap) {
        LOGI("eject", "disarmed (%s)", EjectGesture::cancelName(c));
    }
    const S st = g_.state();
    if (st == S::Pulling || st == S::Fired) {
        lv_obj_set_y(obj_, y0_ + int32_t(g_.pullFrac() * float(g_.params().travelPx)));
    } else if (lv_obj_get_y(obj_) != y0_) {
        lv_obj_set_y(obj_, y0_);
    }
    if (st != shown_) {
        if (st == S::Armed && shown_ != S::Pulling) {
            LOGI("eject", "armed");
            HapticService::click();
        }
        shown_ = st;
        showState();
    }
    if (st == S::Fired) {
        LOGW("eject", "fire");
        EventBus::publish(EventType::EjectRequested, 0, now);
        g_.reset();
        shown_ = S::Idle;
        lv_obj_set_y(obj_, y0_);
        showState();
    }
}

void EjectHandle::showState()
{
    switch (shown_) {
    case S::Armed:
    case S::Pulling:
        lv_label_set_text(label_, "ARMED - PULL");
        break;
    default:
        lv_label_set_text(label_, "PULL TO EJECT");
        lv_obj_set_style_border_width(obj_, 0, 0);
        break;
    }
}
