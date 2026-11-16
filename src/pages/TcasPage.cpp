#include "pages/TcasPage.h"
#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "services/Log.h"
#include "ui/HapticClick.h"
#include "ui/Theme.h"

namespace {

TcasPage* s_self;
constexpr uint32_t kSweepPeriodMs = 50;     // 20 Hz redraw while visible (FR-013)
constexpr uint16_t kSweepStepDeg10 = 18;    // 1.8° per frame → 6 rotations/min
constexpr int kTrail = 8;                   // fading trail lines behind the sweep
constexpr uint32_t kLostShowMs = 3000;

lv_obj_t* plain(lv_obj_t* parent)
{
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    return o;
}

void setText(lv_obj_t* l, char* cache, size_t n, const char* text)
{
    if (strncmp(cache, text, n) != 0) {
        snprintf(cache, n, "%s", text);
        lv_label_set_text(l, text);
    }
}

// Ring k (0 = inner) is drawn at this fraction of the scope radius; bogeys sit on their ring.
float ringFrac(int k) { return 0.25f * float(k + 1); }

} // namespace

void TcasPage::onCreate(lv_obj_t* scr)
{
    s_self = this;
    lv_obj_t* col = plain(scr);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(col, Theme::S(30), 0);   // below the annunciator band
    lv_obj_set_style_pad_bottom(col, Theme::S(14), 0);
    lv_obj_set_style_pad_row(col, Theme::S(3), 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    header_ = lv_label_create(col);
    lv_obj_add_style(header_, Theme::title(), 0);
    lv_label_set_text(header_, "TCAS");

    info_ = lv_label_create(col);
    lv_obj_set_style_text_font(info_, Theme::fontSmall(), 0);
    lv_obj_set_style_text_color(info_, Theme::dim(), 0);
    lv_label_set_text(info_, "");

    const int32_t size = Theme::S(186);
    ppi_ = plain(col);
    lv_obj_set_size(ppi_, size, size);
    lv_obj_remove_flag(ppi_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ppi_, LV_OBJ_FLAG_CLICKABLE);   // a tap on empty scope closes the card
    lv_obj_add_event_cb(ppi_, onCard, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(ppi_, onPpiDraw, LV_EVENT_DRAW_MAIN, this);

    for (int i = 0; i < core::TcasTable::kMax; i++) {
        lv_obj_t* b = plain(ppi_);
        const int32_t d = Theme::S(12);
        lv_obj_set_size(b, d, d);
        lv_obj_set_style_bg_color(b, Theme::hudGreen(), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(b, Theme::S(2), 0);
        lv_obj_set_style_transform_rotation(b, 450, 0);   // a diamond
        lv_obj_set_style_transform_pivot_x(b, d / 2, 0);
        lv_obj_set_style_transform_pivot_y(b, d / 2, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b, Theme::S(12));   // ≥ 48 px hit area on the Ultra
        lv_obj_remove_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(b, onBogey, LV_EVENT_CLICKED, reinterpret_cast<void*>(intptr_t(i)));
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        bogey_[i] = b;
        lv_obj_t* l = lv_label_create(ppi_);
        lv_obj_set_style_text_font(l, Theme::fontSmall(), 0);
        lv_obj_set_style_text_color(l, Theme::hudGreen(), 0);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        bogeyLbl_[i] = l;
    }

    status_ = lv_label_create(ppi_);
    lv_obj_add_style(status_, Theme::body(), 0);
    lv_obj_set_style_text_color(status_, Theme::caution(), 0);
    lv_obj_center(status_);
    lv_label_set_text(status_, "");

    lv_obj_t* grow = plain(col);
    lv_obj_set_width(grow, 1);
    lv_obj_set_flex_grow(grow, 1);

    lv_obj_t* btn = lv_button_create(col);
    lv_obj_add_style(btn, Theme::button(), 0);
    lv_obj_set_size(btn, Theme::S(110), Theme::S(30));   // ≥ 48 px on the Ultra (NFR-USE-001)
    lv_obj_add_event_cb(btn, onToggle, LV_EVENT_CLICKED, nullptr);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    HapticClick::attach(btn);
    toggleLbl_ = lv_label_create(btn);
    lv_label_set_text(toggleLbl_, "TCAS ON");
    lv_obj_center(toggleLbl_);

    // Detail card (FR-008): an overlay over the scope; a tap closes it.
    card_ = lv_obj_create(scr);
    lv_obj_set_size(card_, Theme::S(200), LV_SIZE_CONTENT);
    lv_obj_align(card_, LV_ALIGN_CENTER, 0, Theme::S(10));
    lv_obj_set_style_bg_color(card_, Theme::bg(), 0);
    lv_obj_set_style_bg_opa(card_, LV_OPA_90, 0);
    lv_obj_set_style_border_color(card_, Theme::hudGreen(), 0);
    lv_obj_set_style_border_width(card_, Theme::S(2), 0);
    lv_obj_set_style_radius(card_, Theme::S(8), 0);
    lv_obj_set_style_pad_all(card_, Theme::S(8), 0);
    lv_obj_remove_flag(card_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(card_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(card_, onCard, LV_EVENT_CLICKED, nullptr);
    cardLbl_ = lv_label_create(card_);
    lv_obj_add_style(cardLbl_, Theme::body(), 0);
    lv_obj_set_style_text_color(cardLbl_, Theme::fg(), 0);
    lv_label_set_text(cardLbl_, "");
    lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
}

void TcasPage::onEnter()
{
    sweepFrames_ = 0;
    sweepSinceMs_ = millis();
    if (!sweepTimer_) {
        sweepTimer_ = lv_timer_create(onSweep, kSweepPeriodMs, this);
    }
    refresh(true);
}

void TcasPage::onExit()
{
    if (sweepTimer_) {
        lv_timer_delete(sweepTimer_);
        sweepTimer_ = nullptr;
        const uint32_t dt = millis() - sweepSinceMs_;
        if (dt > 2000) {
            LOGD("tcas", "radar redraw %.1f Hz", sweepFrames_ * 1000.0f / float(dt));
        }
    }
    selected_ = 0;
    lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
}

void TcasPage::onTick(uint32_t) { refresh(false); }

void TcasPage::onSweep(lv_timer_t* t)
{
    auto* self = static_cast<TcasPage*>(lv_timer_get_user_data(t));
    if (!self->active_) {
        return;
    }
    self->sweepDeg10_ = uint16_t((self->sweepDeg10_ + kSweepStepDeg10) % 3600);
    self->sweepFrames_++;
    lv_obj_invalidate(self->ppi_);
}

void TcasPage::onToggle(lv_event_t*)
{
    const TcasSnapshot s = TcasService::status().read();
    TcasService::setEnabled(!s.on);
}

void TcasPage::onBogey(lv_event_t* e)
{
    const int i = int(intptr_t(lv_event_get_user_data(e)));
    s_self->selected_ = s_self->bogeyId_[i];
    s_self->lostAtMs_ = 0;
    lv_obj_remove_flag(s_self->card_, LV_OBJ_FLAG_HIDDEN);
    s_self->refresh(true);
}

void TcasPage::onCard(lv_event_t*)
{
    s_self->selected_ = 0;
    lv_obj_add_flag(s_self->card_, LV_OBJ_FLAG_HIDDEN);
}

void TcasPage::onPpiDraw(lv_event_t* e)
{
    auto* self = static_cast<TcasPage*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(self->ppi_, &a);
    const int32_t cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2;
    const int32_t R = lv_area_get_width(&a) / 2 - Theme::S(2);
    const lv_color_t green = Theme::hudGreen();
    const lv_opa_t ringOpa = self->active_ ? LV_OPA_70 : LV_OPA_30;

    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.center.x = cx;
    ad.center.y = cy;
    ad.start_angle = 0;
    ad.end_angle = 360;
    ad.color = green;
    ad.opa = ringOpa;
    ad.width = 1;
    for (int k = 0; k < core::TcasTable::kRings; k++) {
        ad.radius = uint16_t(R * ringFrac(k));
        ad.width = k == core::TcasTable::kRings - 1 ? 2 : 1;
        lv_draw_arc(layer, &ad);
    }

    // Ownship: a small cross in the centre.
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = Theme::fg();
    ld.width = 2;
    const int32_t o = Theme::S(5);
    ld.p1 = {lv_value_precise_t(cx - o), lv_value_precise_t(cy)};
    ld.p2 = {lv_value_precise_t(cx + o), lv_value_precise_t(cy)};
    lv_draw_line(layer, &ld);
    ld.p1 = {lv_value_precise_t(cx), lv_value_precise_t(cy - o)};
    ld.p2 = {lv_value_precise_t(cx), lv_value_precise_t(cy + o)};
    lv_draw_line(layer, &ld);

    if (!self->active_) {
        return;
    }
    // Sweep with a fading trail.
    ld.color = green;
    for (int k = kTrail; k >= 0; k--) {
        const float deg = (self->sweepDeg10_ - k * 30) / 10.0f;
        const float rad = deg * 3.14159265f / 180.0f;
        ld.width = k == 0 ? 3 : 2;
        ld.opa = lv_opa_t(k == 0 ? LV_OPA_COVER : LV_OPA_60 - k * 6);
        ld.p1 = {lv_value_precise_t(cx), lv_value_precise_t(cy)};
        ld.p2 = {lv_value_precise_t(cx + R * sinf(rad)), lv_value_precise_t(cy - R * cosf(rad))};
        lv_draw_line(layer, &ld);
    }
}

void TcasPage::layoutBogeys(const TcasSnapshot& s)
{
    const int32_t W = lv_obj_get_width(ppi_);
    const int32_t c = W / 2, R = W / 2 - Theme::S(2);
    const int32_t d = Theme::S(12);
    for (int i = 0; i < core::TcasTable::kMax; i++) {
        if (i >= s.n) {
            lv_obj_add_flag(bogey_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(bogeyLbl_[i], LV_OBJ_FLAG_HIDDEN);
            bogeyId_[i] = 0;
            continue;
        }
        const core::Contact& k = s.contacts[i];
        const float rad = k.bearingDeg * 3.14159265f / 180.0f;
        const float r = R * ringFrac(k.ring);
        const int32_t x = c + int32_t(r * sinf(rad)), y = c - int32_t(r * cosf(rad));
        lv_obj_set_pos(bogey_[i], x - d / 2, y - d / 2);
        lv_obj_set_style_bg_color(bogey_[i], k.ring == 0 ? Theme::caution() : Theme::hudGreen(), 0);
        lv_obj_remove_flag(bogey_[i], LV_OBJ_FLAG_HIDDEN);
        if (strcmp(lv_label_get_text(bogeyLbl_[i]), k.callsign) != 0) {
            lv_label_set_text(bogeyLbl_[i], k.callsign);
        }
        lv_obj_set_style_text_color(bogeyLbl_[i], k.ring == 0 ? Theme::caution() : Theme::hudGreen(), 0);
        lv_obj_set_pos(bogeyLbl_[i], x + d / 2 + Theme::S(2), y - d);
        lv_obj_remove_flag(bogeyLbl_[i], LV_OBJ_FLAG_HIDDEN);
        bogeyId_[i] = k.nodeId;
    }
}

void TcasPage::updateCard(const TcasSnapshot& s)
{
    if (selected_ == 0) {
        return;
    }
    char b[160];
    const core::Contact* c = nullptr;
    for (int i = 0; i < s.n; i++) {
        if (s.contacts[i].nodeId == selected_) {
            c = &s.contacts[i];
        }
    }
    const uint32_t now = millis();
    if (!c) {
        if (lostAtMs_ == 0) {
            lostAtMs_ = now;
        }
        if (now - lostAtMs_ >= kLostShowMs) {
            selected_ = 0;
            lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        snprintf(b, sizeof b, "LOST\n%08lX", (unsigned long)selected_);
    } else {
        lostAtMs_ = 0;
        // The id suffix tells two equal callsigns apart (spec edge case).
        snprintf(b, sizeof b, "%s  #%04lX%s\nRSSI %d dBm  SNR %.1f dB\nheard %lu s ago\nBATT %u %%  RING %u",
                 c->callsign, (unsigned long)(c->nodeId & 0xFFFF), c->simulated ? "  SIM" : "", int(c->lastRssi),
                 c->lastSnrQ4 / 4.0f, (unsigned long)((s.atMs - c->lastSeenMs) / 1000), unsigned(c->battPct),
                 unsigned(c->ring));
    }
    if (strcmp(lv_label_get_text(cardLbl_), b) != 0) {
        lv_label_set_text(cardLbl_, b);
    }
}

void TcasPage::refresh(bool force)
{
    const uint32_t now = millis();
    const TcasSnapshot s = TcasService::status().read();
    if (!force && s.seq == lastSeq_ && now - lastRefreshMs_ < 500) {
        return;
    }
    lastSeq_ = s.seq;
    lastRefreshMs_ = now;
    char b[64];
    snprintf(b, sizeof b, "TCAS  %s", s.callsign);
    lv_label_set_text(header_, b);
    snprintf(b, sizeof b, "%s %.3f  TFC %u  TX %lu", s.regionName, s.freqMHz, unsigned(s.n), (unsigned long)s.sent);
    setText(info_, infoText_, sizeof infoText_, b);

    const char* st = !s.radioAvail ? "RADIO N/A" : !s.radioOk ? "RADIO FAIL" : !s.on ? "TCAS OFF" : "";
    setText(status_, statusText_, sizeof statusText_, st);
    lv_label_set_text(toggleLbl_, s.on ? "TCAS OFF" : "TCAS ON");   // the action the button performs
    const bool active = s.on && s.running;
    if (active != active_) {
        active_ = active;
        lv_obj_invalidate(ppi_);
    }
    layoutBogeys(s);
    updateCard(s);
}
