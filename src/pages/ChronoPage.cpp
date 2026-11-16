#include "pages/ChronoPage.h"
#include <cstdio>
#include <cstring>
#include "core/SettingDefs.h"
#include "services/SettingsStore.h"
#include "ui/HapticClick.h"
#include "ui/Theme.h"
#include "ui/fonts/fonts.h"

using core::ChronoModel;
using core::SettingKey;
using M = ChronoModel;

namespace {

ChronoPage* s_self;

lv_obj_t* plain(lv_obj_t* parent)
{
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    return o;
}

lv_obj_t* row(lv_obj_t* parent)
{
    lv_obj_t* r = plain(parent);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

lv_obj_t* button(lv_obj_t* parent, const char* text, int32_t w, lv_event_cb_t cb, void* user, lv_obj_t** lbl = nullptr)
{
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_add_style(b, Theme::button(), 0);
    lv_obj_set_size(b, w, Theme::S(30));   // ≥ 48 px on the Ultra (NFR-USE-001)
    lv_obj_set_style_min_height(b, Theme::S(30), 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);   // a drag on a control is never a page swipe
    HapticClick::attach(b);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (lbl) {
        *lbl = l;
    }
    return b;
}

void setText(lv_obj_t* l, char* cache, size_t n, const char* text)
{
    if (strncmp(cache, text, n) != 0) {
        snprintf(cache, n, "%s", text);
        lv_label_set_text(l, text);
    }
}

bool pomodoro() { return SettingsStore::getBool(SettingKey::ChrMode); }

// The length the presets/−/+ edit: countdown length, or the Pomodoro work length.
SettingKey lenKey() { return pomodoro() ? SettingKey::ChrWork : SettingKey::ChrLen; }

} // namespace

void ChronoPage::presetCb(lv_event_t* e)
{
    SettingsStore::set(lenKey(), int32_t(intptr_t(lv_event_get_user_data(e))));
    ChronoService::post(ChronoService::Cmd::Reconfigure);
}

void ChronoPage::stepCb(lv_event_t* e)
{
    int32_t v = SettingsStore::get(lenKey()) + int32_t(intptr_t(lv_event_get_user_data(e)));
    v = v < 1 ? 1 : (v > 120 ? 120 : v);
    SettingsStore::set(lenKey(), v);
    ChronoService::post(ChronoService::Cmd::Reconfigure);
}

void ChronoPage::modeCb(lv_event_t*)
{
    SettingsStore::set(SettingKey::ChrMode, pomodoro() ? 0 : 1);
    ChronoService::post(ChronoService::Cmd::Reconfigure);
}

void ChronoPage::breakCb(lv_event_t*)
{
    int32_t v = SettingsStore::get(SettingKey::ChrBrk) + 5;
    SettingsStore::set(SettingKey::ChrBrk, v > 15 ? 5 : v);
    ChronoService::post(ChronoService::Cmd::Reconfigure);
}

void ChronoPage::toggleCb(lv_event_t*) { ChronoService::post(ChronoService::Cmd::Toggle); }
void ChronoPage::resetCb(lv_event_t*) { ChronoService::post(ChronoService::Cmd::Reset); }

void ChronoPage::onCreate(lv_obj_t* scr)
{
    s_self = this;
    lv_obj_t* col = plain(scr);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(col, Theme::S(30), 0);   // below the annunciator band
    lv_obj_set_style_pad_bottom(col, Theme::S(16), 0);
    lv_obj_set_style_pad_hor(col, Theme::S(14), 0);
    lv_obj_set_style_pad_row(col, Theme::S(4), 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* t = lv_label_create(col);
    lv_obj_add_style(t, Theme::title(), 0);
    lv_label_set_text(t, "CHRONO");

    tank_.create(col, Theme::S(190), Theme::S(46));

    time_ = lv_label_create(col);
    lv_obj_set_style_text_font(time_, &dseg7_40, 0);
    lv_obj_set_style_text_color(time_, Theme::hudGreen(), 0);
    lv_label_set_text(time_, "--:--");

    phase_ = lv_label_create(col);
    lv_obj_add_style(phase_, Theme::body(), 0);
    lv_label_set_text(phase_, "");

    lv_obj_t* grow = plain(col);
    lv_obj_set_width(grow, 1);
    lv_obj_set_flex_grow(grow, 1);

    // Presets 5/15/25/50 (minutes)
    lv_obj_t* r1 = row(col);
    static const int kPresets[4] = {5, 15, 25, 50};
    for (int i = 0; i < 4; i++) {
        char b[4];
        snprintf(b, sizeof b, "%d", kPresets[i]);
        edit_[i] = button(r1, b, Theme::S(44), presetCb, reinterpret_cast<void*>(intptr_t(kPresets[i])));
    }

    // Custom −/+ and the current length
    lv_obj_t* r2 = row(col);
    edit_[4] = button(r2, LV_SYMBOL_MINUS, Theme::S(44), stepCb, reinterpret_cast<void*>(intptr_t(-1)));
    len_ = lv_label_create(r2);
    lv_obj_add_style(len_, Theme::body(), 0);
    edit_[5] = button(r2, LV_SYMBOL_PLUS, Theme::S(44), stepCb, reinterpret_cast<void*>(intptr_t(1)));

    // Mode and break length
    lv_obj_t* r3 = row(col);
    modeBtn_ = button(r3, "COUNTDOWN", Theme::S(104), modeCb, nullptr, &modeLbl_);
    brkBtn_ = button(r3, "BRK 5", Theme::S(70), breakCb, nullptr, &brkLbl_);
    edit_[6] = modeBtn_;

    // Start/Pause, Reset
    lv_obj_t* r4 = row(col);
    button(r4, LV_SYMBOL_PLAY " START", Theme::S(104), toggleCb, nullptr, &goLbl_);
    button(r4, LV_SYMBOL_STOP " RESET", Theme::S(80), resetCb, nullptr);
}

void ChronoPage::setLocked(bool locked)
{
    if (locked == locked_) {
        return;
    }
    locked_ = locked;
    for (lv_obj_t* o : edit_) {
        if (locked) {
            lv_obj_add_state(o, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(o, LV_STATE_DISABLED);
        }
    }
    if (locked) {
        lv_obj_add_state(brkBtn_, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(brkBtn_, LV_STATE_DISABLED);
    }
}

void ChronoPage::refresh(bool force)
{
    const ChronoSnapshot s = ChronoService::status().read();
    if (!force && s.seq == lastSeq_) {
        return;
    }
    lastSeq_ = s.seq;
    char b[48];

    // MM:SS (ceil of the remaining seconds so 00:00 appears only at expiry)
    const int32_t secs = (s.remainingMs + 999) / 1000;
    snprintf(b, sizeof b, "%02ld:%02ld", long(secs / 60), long(secs % 60));
    setText(time_, timeText_, sizeof timeText_, b);
    tank_.set(s.state == M::State::Expired ? 0.0f : s.fraction);

    const bool pomo = s.mode == M::Mode::Pomodoro;
    if (s.state == M::State::Expired) {
        snprintf(b, sizeof b, "BINGO%s", s.late ? " (LATE)" : "");
    } else if (pomo) {
        snprintf(b, sizeof b, "%s  CYCLE %u%s", s.phase == M::Phase::Break ? "BREAK" : "WORK", unsigned(s.cycles),
                 s.state == M::State::Paused ? "  PAUSED" : "");
    } else {
        snprintf(b, sizeof b, "%s", s.state == M::State::Paused ? "PAUSED" : s.state == M::State::Running ? "RUNNING"
                                                                                                         : "READY");
    }
    setText(phase_, phaseText_, sizeof phaseText_, b);

    const bool settingPomo = SettingsStore::getBool(SettingKey::ChrMode);
    snprintf(b, sizeof b, "%s %ld min", settingPomo ? "WORK" : "TIME", (long)SettingsStore::get(lenKey()));
    setText(len_, lenText_, sizeof lenText_, b);
    lv_label_set_text(modeLbl_, settingPomo ? "POMODORO" : "COUNTDOWN");
    lv_label_set_text_fmt(brkLbl_, "BRK %ld", (long)SettingsStore::get(SettingKey::ChrBrk));
    if (settingPomo) {
        lv_obj_remove_flag(brkBtn_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(brkBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(goLbl_, s.state == M::State::Running ? LV_SYMBOL_PAUSE " PAUSE"
                              : s.state == M::State::Paused ? LV_SYMBOL_PLAY " RESUME"
                                                             : LV_SYMBOL_PLAY " START");
    setLocked(s.state == M::State::Running || s.state == M::State::Paused);
}

void ChronoPage::onTick(uint32_t)
{
    refresh(false);
}
