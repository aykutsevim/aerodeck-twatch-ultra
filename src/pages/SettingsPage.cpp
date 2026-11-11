#include "pages/SettingsPage.h"
#include "board/IBoard.h"
#include "core/Zones.h"
#include <cstring>
#include <ctime>
#include "services/NetService.h"
#include "services/SettingsStore.h"
#include "services/AlertManager.h"
#include "ui/HapticClick.h"
#include "ui/Theme.h"

#ifndef AD_FW_VERSION
#define AD_FW_VERSION "dev"
#endif

using core::SettingKey;

namespace {

lv_obj_t* row(lv_obj_t* parent, const char* label)
{
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(r, Theme::S(4), 0);
    lv_obj_t* l = lv_label_create(r);
    lv_obj_add_style(l, Theme::body(), 0);
    lv_label_set_text(l, label);
    return r;
}

lv_obj_t* smallButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user)
{
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_add_style(b, Theme::button(), 0);
    lv_obj_set_size(b, Theme::S(48), Theme::S(48));
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    HapticClick::attach(b);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

void heading(lv_obj_t* parent, const char* text)
{
    lv_obj_t* h = lv_label_create(parent);
    lv_obj_add_style(h, Theme::title(), 0);
    lv_obj_set_style_pad_top(h, Theme::S(8), 0);
    lv_label_set_text(h, text);
}

void brightStep(lv_event_t* e)
{
    int delta = int(intptr_t(lv_event_get_user_data(e)));
    int32_t v = SettingsStore::get(SettingKey::Bright) + delta;
    SettingsStore::set(SettingKey::Bright, v);   // out-of-range is rejected by the store
}

void overlayChanged(lv_event_t* e)
{
    lv_obj_t* sw = lv_event_get_target_obj(e);
    SettingsStore::set(SettingKey::Overlay, lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0);
}

void logChanged(lv_event_t* e)
{
    lv_obj_t* dd = lv_event_get_target_obj(e);
    SettingsStore::set(SettingKey::LogLvl, int32_t(lv_dropdown_get_selected(dd)));
}

void msgboxConfirm(lv_event_t* e)
{
    SettingsStore::restoreDefaults();
    lv_msgbox_close(static_cast<lv_obj_t*>(lv_event_get_user_data(e)));
}

void msgboxCancel(lv_event_t* e)
{
    lv_msgbox_close(static_cast<lv_obj_t*>(lv_event_get_user_data(e)));
}

void restoreClicked(lv_event_t*)
{
    lv_obj_t* mb = lv_msgbox_create(nullptr);   // modal
    lv_msgbox_add_title(mb, "Restore defaults?");
    lv_msgbox_add_text(mb, "All settings return to their defaults.");
    lv_obj_t* cancel = lv_msgbox_add_footer_button(mb, "Cancel");
    lv_obj_t* ok = lv_msgbox_add_footer_button(mb, "Confirm");
    lv_obj_set_height(cancel, Theme::S(48));
    lv_obj_set_height(ok, Theme::S(48));
    lv_obj_add_event_cb(cancel, msgboxCancel, LV_EVENT_CLICKED, mb);
    lv_obj_add_event_cb(ok, msgboxConfirm, LV_EVENT_CLICKED, mb);
    HapticClick::attach(cancel);
    HapticClick::attach(ok);
    lv_obj_set_width(mb, lv_pct(80));
    lv_obj_center(mb);
}

// ---- S2: network & time zone ----

void setupClicked(lv_event_t*) { NetService::startSetup(); }
void syncClicked(lv_event_t*) { NetService::requestSync(); }

void forgetConfirm(lv_event_t* e)
{
    NetService::forget();
    lv_msgbox_close(static_cast<lv_obj_t*>(lv_event_get_user_data(e)));
}

void forgetClicked(lv_event_t*)
{
    lv_obj_t* mb = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(mb, "Forget network?");
    lv_msgbox_add_text(mb, "The watch will stop syncing time until Wi-Fi is set up again.");
    lv_obj_t* cancel = lv_msgbox_add_footer_button(mb, "Cancel");
    lv_obj_t* ok = lv_msgbox_add_footer_button(mb, "Forget");
    lv_obj_set_height(cancel, Theme::S(48));
    lv_obj_set_height(ok, Theme::S(48));
    lv_obj_add_event_cb(cancel, msgboxCancel, LV_EVENT_CLICKED, mb);
    lv_obj_add_event_cb(ok, forgetConfirm, LV_EVENT_CLICKED, mb);
    HapticClick::attach(cancel);
    HapticClick::attach(ok);
    lv_obj_set_width(mb, lv_pct(80));
    lv_obj_center(mb);
}

void zoneApply(lv_event_t* e)
{
    lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    SettingsStore::set(SettingKey::TzId, int32_t(lv_roller_get_selected(roller)));
    lv_obj_delete(lv_obj_get_parent(roller));
}

void zoneCancel(lv_event_t* e) { lv_obj_delete(static_cast<lv_obj_t*>(lv_event_get_user_data(e))); }

// Modal zone picker: roller with all zones (named first, then fixed offsets), OK / Cancel. FR-013, FR-014.
void zoneClicked(lv_event_t*)
{
    static char* options = nullptr;   // built once (~1.5 KB) and kept
    if (!options) {
        size_t len = 0;
        for (size_t i = 0; i < core::Zones::count(); i++) {
            len += strlen(core::Zones::at(i).name) + 1;
        }
        options = static_cast<char*>(lv_malloc(len + 1));
        char* w = options;
        for (size_t i = 0; i < core::Zones::count(); i++) {
            size_t n = strlen(core::Zones::at(i).name);
            memcpy(w, core::Zones::at(i).name, n);
            w += n;
            *w++ = (i + 1 < core::Zones::count()) ? '\n' : '\0';
        }
    }
    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(box, lv_pct(84), lv_pct(70));
    lv_obj_center(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* roller = lv_roller_create(box);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 5);
    lv_obj_set_width(roller, lv_pct(100));
    lv_roller_set_selected(roller, uint32_t(SettingsStore::get(SettingKey::TzId)), LV_ANIM_OFF);
    lv_obj_t* btns = lv_obj_create(box);
    lv_obj_remove_style_all(btns);
    lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* c = smallButton(btns, "Cancel", zoneCancel, box);
    lv_obj_set_width(c, Theme::S(70));
    lv_obj_t* ok = smallButton(btns, "OK", zoneApply, roller);
    lv_obj_set_width(ok, Theme::S(70));
}

// ---- S3: weather station picker (four A-Z rollers, contracts/wx_page.md) ----

struct StationPicker {
    lv_obj_t* box;
    lv_obj_t* rollers[4];
};
StationPicker s_picker;

void stationApply(lv_event_t*)
{
    char code[5];
    for (int i = 0; i < 4; i++) {
        code[i] = char('A' + lv_roller_get_selected(s_picker.rollers[i]));
    }
    code[4] = '\0';
    SettingsStore::setStr(SettingKey::WxStation, code);   // WeatherService clears the old report and fetches (FR-016)
    lv_obj_delete(s_picker.box);
}

void stationCancel(lv_event_t*) { lv_obj_delete(s_picker.box); }

void stationClicked(lv_event_t*)
{
    static const char kLetters[] = "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
    char cur[8] = {0};
    SettingsStore::getStr(SettingKey::WxStation, cur, sizeof cur);
    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(box, lv_pct(84), lv_pct(62));
    lv_obj_center(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* t = lv_label_create(box);
    lv_obj_add_style(t, Theme::title(), 0);
    lv_label_set_text(t, "Weather station");
    lv_obj_t* wheels = lv_obj_create(box);
    lv_obj_remove_style_all(wheels);
    lv_obj_set_size(wheels, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wheels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheels, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_picker.box = box;
    for (int i = 0; i < 4; i++) {
        lv_obj_t* r = lv_roller_create(wheels);
        lv_roller_set_options(r, kLetters, LV_ROLLER_MODE_INFINITE);
        lv_roller_set_visible_row_count(r, 3);
        lv_obj_set_width(r, Theme::S(40));
        lv_obj_set_style_text_font(r, Theme::fontTitle(), 0);
        const char c = (cur[i] >= 'A' && cur[i] <= 'Z') ? cur[i] : 'A';
        lv_roller_set_selected(r, uint32_t(c - 'A'), LV_ANIM_OFF);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_GESTURE_BUBBLE);
        s_picker.rollers[i] = r;
    }
    lv_obj_t* btns = lv_obj_create(box);
    lv_obj_remove_style_all(btns);
    lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* c = smallButton(btns, "Cancel", stationCancel, nullptr);
    lv_obj_set_width(c, Theme::S(70));
    lv_obj_t* ok = smallButton(btns, "OK", stationApply, nullptr);
    lv_obj_set_width(ok, Theme::S(70));
}

// ---- S4: alerts ----

void vibChanged(lv_event_t* e)
{
    SettingsStore::set(SettingKey::HapticLvl, int32_t(lv_dropdown_get_selected(lv_event_get_target_obj(e))));
}

void quietChanged(lv_event_t* e)
{
    SettingsStore::set(SettingKey::Quiet, lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED) ? 1 : 0);
}

void testAlertClear(lv_timer_t*) { AlertManager::clear(core::AlertId::Test1); }

// Raise a CAUTION for 10 s so the wearer can feel the pattern at the chosen strength.
void testAlertClicked(lv_event_t*)
{
    AlertManager::raise(core::AlertId::Test1, core::AlertLevel::Caution, "TEST ALERT");
    lv_timer_t* t = lv_timer_create(testAlertClear, 10000, nullptr);
    lv_timer_set_repeat_count(t, 1);
}

lv_obj_t* wideButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb)
{
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_add_style(b, Theme::button(), 0);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    HapticClick::attach(b);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

// Interactive controls must not turn a horizontal drag into a page swipe.
void noSwipe(lv_obj_t* o) { lv_obj_remove_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE); }

} // namespace

void SettingsPage::onCreate(lv_obj_t* scr)
{
    lv_obj_t* col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(col, Theme::S(16), 0);
    lv_obj_set_style_pad_top(col, Theme::S(30), 0);   // below caution banner
    lv_obj_set_style_pad_bottom(col, Theme::S(40), 0);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
    lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    heading(col, "SETTINGS");

    lv_obj_t* r = row(col, "Brightness");
    lv_obj_t* ctl = lv_obj_create(r);
    lv_obj_remove_style_all(ctl);
    lv_obj_set_size(ctl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctl, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctl, Theme::S(8), 0);
    smallButton(ctl, LV_SYMBOL_MINUS, brightStep, reinterpret_cast<void*>(intptr_t(-1)));
    brightVal_ = lv_label_create(ctl);
    lv_obj_add_style(brightVal_, Theme::body(), 0);
    smallButton(ctl, LV_SYMBOL_PLUS, brightStep, reinterpret_cast<void*>(intptr_t(1)));

    r = row(col, "Diagnostics overlay");
    overlaySw_ = lv_switch_create(r);
    lv_obj_set_size(overlaySw_, Theme::S(56), Theme::S(30));
    lv_obj_add_event_cb(overlaySw_, overlayChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    HapticClick::attach(overlaySw_, true);
    noSwipe(overlaySw_);

    r = row(col, "Log level");
    logDd_ = lv_dropdown_create(r);
    lv_dropdown_set_options(logDd_, "ERROR\nWARN\nINFO\nDEBUG");
    lv_obj_set_width(logDd_, Theme::S(110));
    lv_obj_set_style_min_height(logDd_, Theme::S(40), 0);
    lv_obj_add_event_cb(logDd_, logChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    HapticClick::attach(logDd_, true);
    noSwipe(logDd_);

    heading(col, "NETWORK & TIME");
    r = row(col, "Network");
    netVal_ = lv_label_create(r);
    lv_obj_add_style(netVal_, Theme::body(), 0);
    r = row(col, "Status");
    netStatus_ = lv_label_create(r);
    lv_obj_add_style(netStatus_, Theme::body(), 0);
    r = row(col, "Last sync");
    lastSync_ = lv_label_create(r);
    lv_obj_add_style(lastSync_, Theme::body(), 0);
    lv_obj_t* nb = lv_obj_create(col);
    lv_obj_remove_style_all(nb);
    lv_obj_set_size(nb, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(nb, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(nb, Theme::S(4), 0);
    wideButton(nb, "Set up Wi-Fi", setupClicked);
    wideButton(nb, "Sync now", syncClicked);
    wideButton(nb, "Forget", forgetClicked);
    r = row(col, "Time zone");
    lv_obj_t* zb = lv_button_create(r);
    lv_obj_add_style(zb, Theme::button(), 0);
    lv_obj_add_event_cb(zb, zoneClicked, LV_EVENT_CLICKED, nullptr);
    HapticClick::attach(zb);
    tzVal_ = lv_label_create(zb);
    lv_obj_center(tzVal_);
    r = row(col, "Weather station");   // S3
    lv_obj_t* wb = lv_button_create(r);
    lv_obj_add_style(wb, Theme::button(), 0);
    lv_obj_add_event_cb(wb, stationClicked, LV_EVENT_CLICKED, nullptr);
    HapticClick::attach(wb);
    wxVal_ = lv_label_create(wb);
    lv_obj_center(wxVal_);

    // S4: ALERTS (FR-013..FR-015)
    heading(col, "ALERTS");
    r = row(col, "Vibration");
    vibDd_ = lv_dropdown_create(r);
    lv_dropdown_set_options(vibDd_, "Off\nLow\nHigh");
    lv_obj_set_width(vibDd_, Theme::S(90));
    lv_obj_set_style_min_height(vibDd_, Theme::S(40), 0);
    lv_obj_add_event_cb(vibDd_, vibChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    HapticClick::attach(vibDd_, true);
    noSwipe(vibDd_);
    r = row(col, "Quiet mode");
    quietSw_ = lv_switch_create(r);
    lv_obj_set_size(quietSw_, Theme::S(56), Theme::S(30));
    lv_obj_add_event_cb(quietSw_, quietChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    HapticClick::attach(quietSw_, true);
    noSwipe(quietSw_);
    lv_obj_t* tb = lv_obj_create(col);
    lv_obj_remove_style_all(tb);
    lv_obj_set_size(tb, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tb, LV_FLEX_FLOW_ROW);
    wideButton(tb, "Test alert", testAlertClicked);

    lv_obj_t* rb = lv_button_create(col);
    lv_obj_add_style(rb, Theme::button(), 0);
    lv_obj_set_width(rb, lv_pct(100));
    lv_obj_set_style_bg_color(rb, lv_color_hex(0x502020), 0);
    lv_obj_add_event_cb(rb, restoreClicked, LV_EVENT_CLICKED, nullptr);
    HapticClick::attach(rb);
    lv_obj_t* rbl = lv_label_create(rb);
    lv_label_set_text(rbl, "Restore defaults");
    lv_obj_center(rbl);

    heading(col, "ABOUT / STATUS");
    lv_obj_t* info = lv_label_create(col);
    lv_obj_add_style(info, Theme::body(), 0);
    lv_label_set_text_fmt(info, "Firmware %s\nBoard %s", AD_FW_VERSION, board().name());

    statusBox_ = lv_obj_create(col);
    lv_obj_remove_style_all(statusBox_);
    lv_obj_set_width(statusBox_, lv_pct(100));
    lv_obj_set_height(statusBox_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(statusBox_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(statusBox_, Theme::S(12), 0);
    lv_obj_set_style_pad_row(statusBox_, Theme::S(2), 0);
    for (uint8_t i = 0; i < uint8_t(core::Periph::Count); i++) {
        lv_obj_t* l = lv_label_create(statusBox_);
        lv_obj_set_style_text_font(l, Theme::fontSmall(), 0);
    }

    created_ = true;
    refresh();
}

void SettingsPage::refresh()
{
    if (!created_) {
        return;
    }
    lv_label_set_text_fmt(brightVal_, "%ld%%", (long)((SettingsStore::get(SettingKey::Bright) + 1) * 20));
    if (SettingsStore::getBool(SettingKey::Overlay)) {
        lv_obj_add_state(overlaySw_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(overlaySw_, LV_STATE_CHECKED);
    }
    lv_dropdown_set_selected(logDd_, uint32_t(SettingsStore::get(SettingKey::LogLvl)));
    lv_label_set_text(tzVal_, core::Zones::at(size_t(SettingsStore::get(SettingKey::TzId))).name);
    char stn[8];
    SettingsStore::getStr(SettingKey::WxStation, stn, sizeof stn);
    lv_label_set_text(wxVal_, stn);
    lv_dropdown_set_selected(vibDd_, uint32_t(SettingsStore::get(SettingKey::HapticLvl)));
    if (SettingsStore::getBool(SettingKey::Quiet)) {
        lv_obj_add_state(quietSw_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(quietSw_, LV_STATE_CHECKED);
    }

    // S2: network & sync status (FR-017)
    char ssid[33];
    SettingsStore::getStr(SettingKey::WifiSsid, ssid, sizeof ssid);
    lv_label_set_text(netVal_, ssid[0] ? ssid : "not set");
    NetStatus st = NetService::status().read();
    if (st.busy != NetBusy::Idle) {
        lv_label_set_text(netStatus_, NetService::busyName(st.busy));
    } else {
        lv_label_set_text(netStatus_, st.lastResult == NetResult::None ? "-" : NetService::resultName(st.lastResult));
    }
    const int64_t last = int64_t(SettingsStore::get(SettingKey::LastSync));
    if (last <= 0) {
        lv_label_set_text(lastSync_, "never");
    } else {
        const int64_t age = int64_t(time(nullptr)) - last;
        if (age < 3600) {
            lv_label_set_text_fmt(lastSync_, "%ld min ago", long(age / 60));
        } else {
            lv_label_set_text_fmt(lastSync_, "%ld h ago", long(age / 3600));
        }
    }

    for (uint8_t i = 0; i < uint8_t(core::Periph::Count); i++) {
        lv_obj_t* l = lv_obj_get_child(statusBox_, i);
        core::PeriphState s = board().periph(core::Periph(i));
        lv_color_t c = s == core::PeriphState::Ok       ? Theme::hudGreen()
                       : s == core::PeriphState::Absent ? Theme::dim()
                                                        : Theme::caution();
        lv_obj_set_style_text_color(l, c, 0);
        lv_label_set_text_fmt(l, "%s %s", core::periphName(core::Periph(i)), core::periphStateName(s));
    }
}

void SettingsPage::onTick(uint32_t dtMs)
{
    tickAcc_ += dtMs;
    if (tickAcc_ >= 10000) {   // "Last sync" age (FR-017)
        tickAcc_ = 0;
        refresh();
    }
}
