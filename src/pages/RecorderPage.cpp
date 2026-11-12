#include "pages/RecorderPage.h"
#include <cstdio>
#include <cstring>
#include "core/FlightMath.h"
#include "services/RecorderService.h"
#include "ui/Theme.h"
#include "ui/fonts/fonts.h"

using core::DayLog;
using core::DayRecord;
namespace FM = core::FlightMath;

namespace {

const char* const kWdLetter[7] = {"M", "T", "W", "T", "F", "S", "S"};
const char* const kWdShort[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char* const kMon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool dated(int32_t day) { return day != core::kUndated && day != core::kEmptyDay; }

lv_obj_t* plain(lv_obj_t* parent)
{
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    return o;
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, "");
    return l;
}

} // namespace

void RecorderPage::onCreate(lv_obj_t* scr)
{
    lv_obj_t* col = plain(scr);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(col, Theme::S(30), 0);      // below the annunciator band, clear of the corners
    lv_obj_set_style_pad_bottom(col, Theme::S(26), 0);
    lv_obj_set_style_pad_hor(col, Theme::S(22), 0);
    lv_obj_set_style_pad_row(col, Theme::S(3), 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* t = lv_label_create(col);
    lv_obj_add_style(t, Theme::title(), 0);
    lv_label_set_text(t, "RECORDER");

    // Today's Flight Miles, big HUD digits
    lv_obj_t* nmRow = plain(col);
    lv_obj_set_size(nmRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(nmRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nmRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(nmRow, Theme::S(6), 0);
    nm_ = label(nmRow, &dseg7_40, Theme::hudGreen());
    lv_obj_t* unit = label(nmRow, Theme::fontBody(), Theme::dim());
    lv_label_set_text(unit, "NM");

    steps_ = label(col, Theme::fontBody(), Theme::fg());
    maxg_ = label(col, Theme::fontBody(), Theme::fg());
    note_ = label(col, Theme::fontSmall(), Theme::caution());
    lv_label_set_text(note_, "Motion sensor unavailable");
    lv_obj_add_flag(note_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* grow = plain(col);   // pushes the chart to the bottom
    lv_obj_set_width(grow, 1);
    lv_obj_set_flex_grow(grow, 1);

    selected_ = label(col, Theme::fontSmall(), Theme::dim());

    // 7-day chart: oldest on the left, today on the right
    lv_obj_t* chart = plain(col);
    lv_obj_set_size(chart, lv_pct(100), Theme::S(78));
    lv_obj_set_flex_flow(chart, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chart, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    barMaxH_ = Theme::S(78) - Theme::S(20);
    for (int i = 0; i < kBars; i++) {
        lv_obj_t* s = plain(chart);
        lv_obj_set_size(s, Theme::S(22), lv_pct(100));
        lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(s, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(s, Theme::S(2), 0);
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);   // tap = select; gestures still bubble (page swipes)
        lv_obj_add_event_cb(s, barClicked, LV_EVENT_CLICKED, this);
        lv_obj_t* b = plain(s);
        lv_obj_set_size(b, Theme::S(14), Theme::S(2));
        lv_obj_set_style_radius(b, Theme::S(2), 0);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* w = label(s, Theme::fontSmall(), Theme::dim());
        slot_[i] = s;
        bar_[i] = b;
        wd_[i] = w;
    }
    refresh(true);
}

void RecorderPage::onTick(uint32_t) { refresh(false); }

void RecorderPage::setText(lv_obj_t* l, char* cache, size_t n, const char* text)
{
    if (strcmp(cache, text) != 0) {
        snprintf(cache, n, "%s", text);
        lv_label_set_text(l, text);
    }
}

void RecorderPage::barClicked(lv_event_t* e)
{
    auto* self = static_cast<RecorderPage*>(lv_event_get_user_data(e));
    lv_obj_t* target = lv_event_get_current_target_obj(e);
    for (int i = 0; i < kBars; i++) {
        if (self->slot_[i] == target) {
            self->selectedIdx_ = kBars - 1 - i;   // slot 6 (right) = today
        }
    }
    self->refresh(true);
}

void RecorderPage::showSelected()
{
    const DayRecord& d = shown_[selectedIdx_];
    char nm[12], g[12], date[20], b[64];
    if (dated(d.day)) {
        int y, m, dd;
        DayLog::civil(d.day, y, m, dd);
        snprintf(date, sizeof date, "%s %d %s", kWdShort[DayLog::weekday(d.day)], dd, kMon[(m - 1) % 12]);
    } else {
        snprintf(date, sizeof date, "%s", selectedIdx_ == 0 ? "Today" : "-");
    }
    if (d.steps == 0 && d.maxGc == 0) {
        snprintf(b, sizeof b, "%s  no data", date);
    } else {
        FM::formatNm(FM::nm(d.steps, shownStride_), true, nm, sizeof nm);
        FM::formatG(d.maxGc, d.maxGc > 0, false, g, sizeof g);
        snprintf(b, sizeof b, "%s  %s NM  %lu st  %s G", date, nm, (unsigned long)d.steps, g);
    }
    setText(selected_, selText_, sizeof selText_, b);
}

void RecorderPage::refresh(bool force)
{
    const Snapshot<RecorderSnapshot>& rs = RecorderService::status();
    if (!force && rs.seq() == lastSeq_) {
        return;
    }
    lastSeq_ = rs.seq();
    const RecorderSnapshot s = rs.read();
    char v[16], b[40];

    FM::formatNm(FM::nm(s.steps, s.strideCm), s.stepsOk, v, sizeof v);
    setText(nm_, nmText_, sizeof nmText_, v);
    if (s.stepsOk) {
        snprintf(b, sizeof b, "STEPS  %lu", (unsigned long)s.steps);
    } else {
        snprintf(b, sizeof b, "STEPS  --");
    }
    setText(steps_, stepsText_, sizeof stepsText_, b);
    FM::formatG(s.maxGc, s.accelOk, s.maxGSat, v, sizeof v);   // "--" when the sensor is unavailable (FR-005)
    if (s.accelOk && s.maxGMin >= 0) {
        snprintf(b, sizeof b, "MAX G  %s @ %02d:%02d", v, s.maxGMin / 60, s.maxGMin % 60);
    } else {
        snprintf(b, sizeof b, "MAX G  %s", v);
    }
    setText(maxg_, maxgText_, sizeof maxgText_, b);
    if (s.stepsOk && s.accelOk) {
        lv_obj_add_flag(note_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(note_, LV_OBJ_FLAG_HIDDEN);
    }

    const bool chartChanged = force || s.strideCm != shownStride_ || memcmp(s.chart, shown_, sizeof shown_) != 0;
    if (chartChanged) {
        memcpy(shown_, s.chart, sizeof shown_);
        shownStride_ = s.strideCm;
        uint32_t maxSteps = 0;
        for (const DayRecord& d : shown_) {
            maxSteps = d.steps > maxSteps ? d.steps : maxSteps;
        }
        for (int i = 0; i < kBars; i++) {
            const int idx = kBars - 1 - i;   // slot 0 = 6 days ago, slot 6 = today
            const DayRecord& d = shown_[idx];
            // heights ∝ NM; NM ∝ steps for a given stride, so steps scale identically
            int32_t h = maxSteps ? int32_t(uint64_t(d.steps) * uint64_t(barMaxH_) / maxSteps) : 0;
            const bool empty = d.steps == 0;
            if (h < Theme::S(3)) h = Theme::S(3);
            lv_obj_set_height(bar_[i], h);
            const lv_color_t c = idx == 0 ? Theme::hudGreen() : Theme::accent();
            lv_obj_set_style_bg_color(bar_[i], c, 0);
            lv_obj_set_style_bg_opa(bar_[i], empty ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(bar_[i], c, 0);
            lv_obj_set_style_border_width(bar_[i], empty ? 1 : 0, 0);
            lv_label_set_text(wd_[i], dated(d.day) ? kWdLetter[DayLog::weekday(d.day)] : "-");
        }
    }
    for (int i = 0; i < kBars; i++) {
        lv_obj_set_style_text_color(wd_[i], (kBars - 1 - i) == selectedIdx_ ? Theme::fg() : Theme::dim(), 0);
    }
    showSelected();
}
