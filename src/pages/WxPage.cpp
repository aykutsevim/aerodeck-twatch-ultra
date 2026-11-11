#include "pages/WxPage.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "core/Metar.h"
#include "services/Log.h"
#include "services/NetService.h"
#include "services/TimeService.h"
#include "services/WeatherService.h"
#include "ui/HapticClick.h"
#include "ui/Theme.h"

namespace Metar = core::Metar;

namespace {

constexpr uint32_t kStaleRecheckMs = 30000;   // FR-011: marker updates within 60 s
constexpr float kPi = 3.14159265f;
constexpr int32_t kBtnH = 40;      // REFRESH height (base px): 15 % smaller than the 48 theme minimum it had
constexpr int32_t kBtnBottom = 8;  // gap below the button (base px); clears the rounded corners at 60 % width

lv_obj_t* bare(lv_obj_t* parent)
{
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
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

// "KEY value" line: key in small dim text, value in body text (wraps when long).
lv_obj_t* field(lv_obj_t* parent, const char* key, int32_t width)
{
    lv_obj_t* r = bare(parent);
    lv_obj_set_size(r, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(r, Theme::S(4), 0);
    lv_obj_t* k = label(r, Theme::fontSmall(), Theme::dim());
    lv_label_set_text(k, key);
    lv_obj_t* v = label(r, Theme::fontBody(), Theme::fg());
    lv_obj_set_flex_grow(v, 1);
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    return v;
}

void refreshCb(lv_event_t* e) { static_cast<WxPage*>(lv_event_get_user_data(e))->refreshTapped(); }

const char* errorText(WxResult r)
{
    switch (r) {
    case WxResult::NoWifi: return "NO WIFI";
    case WxResult::NetFail: return "NET FAIL";
    case WxResult::NoReport: return "NO REPORT";
    case WxResult::ServerError: return "SRV ERROR";
    default: return nullptr;
    }
}

} // namespace

void WxPage::onCreate(lv_obj_t* scr)
{
    const int32_t padH = Theme::S(16);
    const int32_t inner = Theme::width() - 2 * padH;

    lv_obj_t* col = bare(scr);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(col, padH, 0);
    lv_obj_set_style_pad_top(col, Theme::S(28), 0);   // below the caution banner; clears the rounded corners
    // The REFRESH button is pinned to the bottom outside the flex flow; reserve its height here so the raw box
    // (flex_grow) ends above it instead of running underneath.
    lv_obj_set_style_pad_bottom(col, Theme::S(kBtnBottom + kBtnH + 2), 0);
    lv_obj_set_style_pad_row(col, Theme::S(3), 0);

    // Header: station, observation time, category pill
    lv_obj_t* hdr = bare(col);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    stn_ = label(hdr, Theme::fontTitle(), Theme::fg());
    obs_ = label(hdr, Theme::fontBody(), Theme::fg());
    pill_ = bare(hdr);
    lv_obj_set_size(pill_, Theme::S(52), Theme::S(22));
    lv_obj_set_style_radius(pill_, Theme::S(11), 0);
    lv_obj_set_style_bg_opa(pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(pill_, Theme::catUnknown(), 0);
    pillTxt_ = label(pill_, Theme::fontBody(), lv_color_black());
    lv_obj_center(pillTxt_);

    // Annunciator row: STALE / error (amber box) and fetched time / REFRESHING...
    lv_obj_t* an = bare(col);
    lv_obj_set_size(an, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(an, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(an, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    annun_ = label(an, Theme::fontSmall(), Theme::caution());
    lv_obj_set_style_border_color(annun_, Theme::caution(), 0);
    lv_obj_set_style_border_width(annun_, 2, 0);
    lv_obj_set_style_pad_hor(annun_, Theme::S(4), 0);
    lv_obj_set_style_pad_ver(annun_, Theme::S(1), 0);
    upd_ = label(an, Theme::fontSmall(), Theme::dim());

    // Middle: compass + WIND / VIS / QNH
    lv_obj_t* mid = bare(col);
    lv_obj_set_size(mid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mid, Theme::S(8), 0);

    roseD_ = Theme::S(72);
    rose_ = bare(mid);
    lv_obj_set_size(rose_, roseD_, roseD_);
    lv_obj_set_style_radius(rose_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(rose_, 2, 0);
    lv_obj_set_style_border_color(rose_, Theme::dim(), 0);
    const char* card[] = {"N", "E", "S", "W"};
    const lv_align_t al[] = {LV_ALIGN_TOP_MID, LV_ALIGN_RIGHT_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_LEFT_MID};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* c = label(rose_, Theme::fontSmall(), i == 0 ? Theme::fg() : Theme::dim());
        lv_label_set_text(c, card[i]);
        lv_obj_align(c, al[i], 0, 0);
    }
    sector_ = lv_arc_create(rose_);
    lv_obj_set_size(sector_, roseD_ - Theme::S(10), roseD_ - Theme::S(10));
    lv_obj_center(sector_);
    lv_arc_set_bg_angles(sector_, 0, 0);
    lv_obj_remove_style(sector_, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(sector_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(sector_, Theme::S(3), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sector_, Theme::advisory(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(sector_, LV_OPA_TRANSP, LV_PART_MAIN);
    arrow_ = lv_line_create(rose_);
    lv_obj_set_style_line_width(arrow_, Theme::S(3), 0);
    lv_obj_set_style_line_color(arrow_, Theme::hudGreen(), 0);
    lv_obj_set_style_line_rounded(arrow_, true, 0);
    head_ = lv_line_create(rose_);
    lv_obj_set_style_line_width(head_, Theme::S(3), 0);
    lv_obj_set_style_line_color(head_, Theme::hudGreen(), 0);
    lv_obj_set_style_line_rounded(head_, true, 0);
    centre_ = label(rose_, Theme::fontSmall(), Theme::fg());
    lv_obj_center(centre_);

    const int32_t rightW = inner - roseD_ - Theme::S(8);
    lv_obj_t* right = bare(mid);
    lv_obj_set_size(right, rightW, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, Theme::S(2), 0);
    wind_ = field(right, "WIND", rightW);
    vis_ = field(right, "VIS", rightW);
    qnh_ = field(right, "QNH", rightW);
    tdp_ = field(right, "TEMP", rightW);

    // T/DP and CLOUDS
    lv_obj_t* row2 = bare(col);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    clouds_ = field(row2, "CLOUDS", inner);

    // Raw METAR: wraps; the box scrolls vertically when the text is longer than the space left.
    rawBox_ = lv_obj_create(col);
    lv_obj_remove_style_all(rawBox_);
    lv_obj_set_width(rawBox_, lv_pct(100));
    lv_obj_set_flex_grow(rawBox_, 1);
    lv_obj_set_style_border_width(rawBox_, 1, 0);
    lv_obj_set_style_border_color(rawBox_, Theme::dim(), 0);
    lv_obj_set_style_radius(rawBox_, Theme::S(4), 0);
    lv_obj_set_style_pad_all(rawBox_, Theme::S(3), 0);
    lv_obj_set_scroll_dir(rawBox_, LV_DIR_VER);
    raw_ = label(rawBox_, Theme::fontSmall(), Theme::fg());
    lv_obj_set_width(raw_, lv_pct(100));
    lv_label_set_long_mode(raw_, LV_LABEL_LONG_WRAP);

    // REFRESH
    btn_ = lv_button_create(scr);
    lv_obj_add_style(btn_, Theme::button(), 0);
    lv_obj_set_size(btn_, lv_pct(51), Theme::S(kBtnH));   // 15 % smaller than the first 60 % x 48
    lv_obj_set_style_min_height(btn_, 0, 0);               // the theme's 48 minimum would override the height
    lv_obj_align(btn_, LV_ALIGN_BOTTOM_MID, 0, -Theme::S(kBtnBottom));
    lv_obj_set_style_bg_color(btn_, Theme::dim(), LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn_, refreshCb, LV_EVENT_CLICKED, this);
    HapticClick::attach(btn_);
    lv_obj_t* bl = lv_label_create(btn_);
    lv_label_set_text(bl, LV_SYMBOL_REFRESH " REFRESH");
    lv_obj_center(bl);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    created_ = true;
    update(true);
    // Size the raw box from absolute coordinates: it must end above REFRESH whatever the flex layout decided.
    lv_obj_update_layout(scr);
    lv_area_t box, btn;
    lv_obj_get_coords(rawBox_, &box);
    lv_obj_get_coords(btn_, &btn);
    const int32_t h = btn.y1 - Theme::S(4) - box.y1;
    lv_obj_set_flex_grow(rawBox_, 0);
    lv_obj_set_height(rawBox_, h > Theme::S(20) ? h : Theme::S(20));
    lv_obj_update_layout(scr);
    lv_obj_get_coords(rawBox_, &box);
    LOGI("wx", "layout: raw box y=%ld..%ld, button y=%ld..%ld (absolute)", (long)box.y1, (long)box.y2, (long)btn.y1,
         (long)btn.y2);
}

void WxPage::refreshTapped()
{
    const WxBusy busy = WeatherService::status().read().busy;
    if (busy != WxBusy::Idle || refreshing_) {
        return;   // FR-015: no extra fetch while one is pending/running
    }
    if (!NetService::requestWeather()) {
        WeatherService::markNoWifi();
        noWifiFlash_ = true;
    } else {
        refreshing_ = true;
        refreshSeq_ = WeatherService::status().read().seq;   // publish counter after markPending()
        noWifiFlash_ = false;
    }
    update(true);   // feedback in this LVGL cycle (≤100 ms)
}

void WxPage::onTick(uint32_t dtMs)
{
    staleAcc_ += dtMs;
    const bool due = staleAcc_ >= kStaleRecheckMs;
    if (due) {
        staleAcc_ = 0;
    }
    update(due);
}

void WxPage::updateCompass(const WeatherSnapshot& s)
{
    const Metar::Report& r = s.report;
    const bool have = s.hasReport && r.windSpdKt >= 0;
    const bool calm = have && r.windSpdKt == 0 && (r.windDir == 0 || r.windDir == Metar::kWindVrb);
    const bool vrb = have && !calm && (r.windDir == Metar::kWindVrb || r.windVarFrom >= 0);
    const bool arrow = have && !calm && r.windDir >= 0 && r.windVarFrom < 0;

    if (!have) {
        lv_label_set_text(centre_, "--");
    } else if (calm) {
        lv_label_set_text(centre_, "CALM");
    } else if (vrb) {
        lv_label_set_text_fmt(centre_, "VRB\n%d", r.windSpdKt);
    } else {
        lv_label_set_text_fmt(centre_, "%d", r.windSpdKt);
    }
    lv_obj_set_style_text_align(centre_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(centre_);

    if (have && !calm && r.windVarFrom >= 0) {
        // lv_arc: 0° = 3 o'clock, clockwise. Bearing b → b - 90.
        int32_t a0 = (r.windVarFrom + 270) % 360, a1 = (r.windVarTo + 270) % 360;
        lv_arc_set_angles(sector_, lv_value_precise_t(a0), lv_value_precise_t(a1));
        lv_obj_remove_flag(sector_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(sector_, LV_OBJ_FLAG_HIDDEN);
    }

    if (!arrow) {
        lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(head_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    // Wind FROM bearing: the arrow starts on the rim at that bearing and points to the centre (where it blows).
    const float c = roseD_ / 2.0f;
    const float rad = c - Theme::S(10);
    const float b = r.windDir * kPi / 180.0f;
    const float px = c + rad * sinf(b), py = c - rad * cosf(b);
    const float tx = c + rad * 0.15f * sinf(b), ty = c - rad * 0.15f * cosf(b);   // tip just short of centre
    arrowPts_[0] = {lv_value_precise_t(px), lv_value_precise_t(py)};
    arrowPts_[1] = {lv_value_precise_t(tx), lv_value_precise_t(ty)};
    // Arrowhead: two strokes back from the tip at ±28°.
    const float ux = tx - px, uy = ty - py;
    const float len = sqrtf(ux * ux + uy * uy);
    const float nx = ux / len, ny = uy / len;
    const float hl = Theme::S(9);
    const float ca = cosf(0.49f), sa = sinf(0.49f);
    const float l1x = tx - hl * (nx * ca - ny * sa), l1y = ty - hl * (nx * sa + ny * ca);
    const float l2x = tx - hl * (nx * ca + ny * sa), l2y = ty - hl * (-nx * sa + ny * ca);
    headPts_[0] = {lv_value_precise_t(l1x), lv_value_precise_t(l1y)};
    headPts_[1] = {lv_value_precise_t(tx), lv_value_precise_t(ty)};
    headPts_[2] = {lv_value_precise_t(l2x), lv_value_precise_t(l2y)};
    lv_line_set_points(arrow_, arrowPts_, 2);
    lv_line_set_points(head_, headPts_, 3);
    lv_obj_remove_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(head_, LV_OBJ_FLAG_HIDDEN);
}

void WxPage::update(bool force)
{
    if (!created_) {
        return;
    }
    const uint32_t seq = WeatherService::status().seq();
    if (!force && seq == lastSeq_) {
        return;
    }
    lastSeq_ = seq;
    static WeatherSnapshot s;   // ~450 B, UI task only
    s = WeatherService::status().read();
    const ClockSnapshot clk = TimeService::clock().read();
    const NetStatus net = NetService::status().read();

    if (refreshing_ && s.busy == WxBusy::Idle && s.seq != refreshSeq_) {
        refreshing_ = false;
    }
    const bool busy = refreshing_ || s.busy != WxBusy::Idle;
    if (busy) {
        lv_obj_add_state(btn_, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(btn_, LV_STATE_DISABLED);
    }

    lv_label_set_text(stn_, s.station);
    const Metar::Report& r = s.report;
    if (s.hasReport) {
        lv_label_set_text_fmt(obs_, "%02d%02dZ", r.hour, r.minute);
        const Metar::Category cat = Metar::category(r);
        lv_obj_set_style_bg_color(pill_, Theme::catColor(cat), 0);
        lv_label_set_text(pillTxt_, Metar::categoryName(cat));
    } else {
        lv_label_set_text(obs_, "NO DATA");
        lv_obj_set_style_bg_color(pill_, Theme::catUnknown(), 0);
        lv_label_set_text(pillTxt_, "--");
    }

    // Annunciators: STALE (only with a report) and the failure reason.
    char an[32] = "";
    const bool stale = s.hasReport && WeatherService::isStale(s, clk.utc, clk.valid);
    const char* err = !net.configured ? "NO WIFI" : errorText(s.lastResult);
    if (noWifiFlash_ && !err) {
        err = "NO WIFI";
    }
    if (stale && err) {
        snprintf(an, sizeof an, "STALE  %s", err);
    } else if (stale) {
        snprintf(an, sizeof an, "STALE");
    } else if (err) {
        snprintf(an, sizeof an, "%s", err);
    }
    lv_label_set_text(annun_, an);
    if (an[0]) {
        lv_obj_remove_flag(annun_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(annun_, LV_OBJ_FLAG_HIDDEN);
    }
    if (busy) {
        lv_label_set_text(upd_, "REFRESHING...");
    } else if (s.fetchedUtc > 0) {
        const int64_t t = s.fetchedUtc % 86400;
        lv_label_set_text_fmt(upd_, "upd %02d:%02dZ", int(t / 3600), int((t / 60) % 60));
    } else {
        lv_label_set_text(upd_, "");
    }

    char b[96];
    if (s.hasReport) {
        Metar::formatWind(r, b, sizeof b);
        lv_label_set_text(wind_, b);
        lv_label_set_text(vis_, r.visText[0] ? r.visText : "--");
        if (r.qnhHpa >= 0) {
            lv_label_set_text_fmt(qnh_, "%d hPa", r.qnhHpa);
        } else {
            lv_label_set_text(qnh_, "--");
        }
        Metar::formatTemp(r, b, sizeof b);
        lv_label_set_text(tdp_, b);
        Metar::formatCloudsText(r, b, sizeof b);
        lv_label_set_text(clouds_, b);
        lv_label_set_text(raw_, s.raw);
    } else {
        for (lv_obj_t* l : {wind_, vis_, qnh_, tdp_, clouds_}) {
            lv_label_set_text(l, "--");
        }
        lv_label_set_text(raw_, "");
    }
    updateCompass(s);
}
