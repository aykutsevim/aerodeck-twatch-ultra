#include "pages/PfdPage.h"
#include <sys/time.h>
#include "core/ClockMath.h"
#include "services/AttitudeService.h"
#include "services/PowerService.h"
#include "services/TimeService.h"
#include "ui/Theme.h"
#include "ui/fonts/fonts.h"

lv_obj_t* PfdPage::corner(lv_obj_t* scr, const core::Box& b, const char* caption, lv_obj_t** value)
{
    lv_obj_t* c = lv_obj_create(scr);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, b.x, b.y);
    lv_obj_set_size(c, b.w, b.h);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* cap = lv_label_create(c);
    lv_obj_set_style_text_font(cap, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(cap, Theme::dim(), 0);
    lv_label_set_text(cap, caption);
    *value = lv_label_create(c);
    lv_obj_set_style_text_font(*value, &dseg7_28, 0);
    lv_obj_set_style_text_color(*value, Theme::hudGreen(), 0);
    lv_label_set_text(*value, "--:--");
    return c;
}

lv_obj_t* PfdPage::smallLabel(lv_obj_t* scr, const core::Box& b, const char* text, lv_align_t align)
{
    lv_obj_t* l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(l, Theme::dim(), 0);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, b.x, b.y);
    lv_obj_set_size(l, b.w, b.h);
    lv_obj_set_style_text_align(l, align == LV_ALIGN_TOP_LEFT ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
    return l;
}

void PfdPage::onCreate(lv_obj_t* scr)
{
    layout_ = core::PfdLayout::compute(Theme::width(), Theme::height(), Theme::cornerRadius());
    const core::PfdLayout& L = layout_;
    heading_.create(scr, TapeWidget::Dir::Horizontal, L.heading.x, L.heading.y, L.heading.w, L.heading.h, 360,
                    nullptr);
    hour_.create(scr, TapeWidget::Dir::Vertical, L.hour.x, L.hour.y, L.hour.w, L.hour.h, 24, "HR");
    minute_.create(scr, TapeWidget::Dir::Vertical, L.minute.x, L.minute.y, L.minute.w, L.minute.h, 60, "MIN");
    adi_.create(scr, L.adi.x, L.adi.y, L.adi.w);
    eicas_.create(scr, L.eicas.x, L.eicas.y, L.eicas.w, L.eicas.h);
    corner(scr, L.zulu, "ZULU", &zulu_);
    corner(scr, L.local, "LOCAL", &local_);
    // Placeholders filled by the flight recorder (slice S6, FR-017).
    smallLabel(scr, L.maxg, "MAX G\n--", LV_ALIGN_TOP_LEFT);
    smallLabel(scr, L.nm, "FLIGHT NM\n--", LV_ALIGN_TOP_RIGHT);
    lv_display_add_event_cb(lv_display_get_default(), renderReadyCb, LV_EVENT_RENDER_READY, this);
}

void PfdPage::onEnter()
{
    frame(true);   // FR-021: no stale values when returning to the PFD
    if (!frame_) {
        frame_ = lv_timer_create(frameCb, 16, this);
    }
}

void PfdPage::onExit()
{
    if (frame_) {
        lv_timer_delete(frame_);
        frame_ = nullptr;
    }
}

void PfdPage::frameCb(lv_timer_t* t)
{
    static_cast<PfdPage*>(lv_timer_get_user_data(t))->frame(false);
}

void PfdPage::frame(bool force)
{
    adi_.set(AttitudeService::att().read());

    // Time: heading every frame from ms; tapes and corners only when the minute (or validity/tz) changes.
    ClockSnapshot c = TimeService::clock().read();
    if (c.valid) {
        timeval tv;
        gettimeofday(&tv, nullptr);
        core::LocalTime local = core::splitLocal(int64_t(tv.tv_sec), uint16_t(tv.tv_usec / 1000), c.tzOffsetMin);
        heading_.setHeading(local.headingDeg);
        int key = int((int64_t(tv.tv_sec) / 60) % 100000) * 2048 + (c.tzOffsetMin + 1024);
        if (force || key != lastMinuteKey_) {
            lastMinuteKey_ = key;
            core::LocalTime utc = core::splitLocal(int64_t(tv.tv_sec), 0, 0);
            hour_.setValue(local.h);
            minute_.setValue(local.m);
            lv_label_set_text_fmt(zulu_, "%02u:%02u", utc.h, utc.m);
            lv_label_set_text_fmt(local_, "%02u:%02u", local.h, local.m);
        }
    } else if (force || lastMinuteKey_ != -2) {
        lastMinuteKey_ = -2;   // FR-010: never show an invented time
        heading_.setHeading(-1.0f);
        hour_.setValue(-1);
        minute_.setValue(-1);
        lv_label_set_text(zulu_, "--:--");
        lv_label_set_text(local_, "--:--");
    }

    const Snapshot<PowerSnapshot>& ps = PowerService::power();
    if (force || ps.seq() != lastPowerSeq_) {
        lastPowerSeq_ = ps.seq();
        eicas_.set(ps.read());
    }
}

// Latency (research R9): report once per horizon redraw, when the frame containing it has been flushed.
void PfdPage::renderReadyCb(lv_event_t* e)
{
    auto* self = static_cast<PfdPage*>(lv_event_get_user_data(e));
    if (!self->frame_) {
        return;
    }
    uint32_t ms = self->adi_.drawnSampleMs();
    if (ms != 0 && ms != self->lastNotifiedSampleMs_) {
        self->lastNotifiedSampleMs_ = ms;
        AttitudeService::notifyRendered(ms);
    }
}
