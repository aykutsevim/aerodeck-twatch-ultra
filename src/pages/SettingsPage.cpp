#include "pages/SettingsPage.h"
#include "board/IBoard.h"
#include "services/SettingsStore.h"
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

void tzStep(lv_event_t* e)
{
    int delta = int(intptr_t(lv_event_get_user_data(e)));
    SettingsStore::set(SettingKey::TzOff, SettingsStore::get(SettingKey::TzOff) + delta * 30);   // range-checked
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
    lv_obj_set_width(mb, lv_pct(80));
    lv_obj_center(mb);
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
    noSwipe(overlaySw_);

    r = row(col, "Log level");
    logDd_ = lv_dropdown_create(r);
    lv_dropdown_set_options(logDd_, "ERROR\nWARN\nINFO\nDEBUG");
    lv_obj_set_width(logDd_, Theme::S(110));
    lv_obj_set_style_min_height(logDd_, Theme::S(40), 0);
    lv_obj_add_event_cb(logDd_, logChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    noSwipe(logDd_);

    r = row(col, "Local offset");
    lv_obj_t* tzCtl = lv_obj_create(r);
    lv_obj_remove_style_all(tzCtl);
    lv_obj_set_size(tzCtl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tzCtl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tzCtl, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tzCtl, Theme::S(8), 0);
    smallButton(tzCtl, LV_SYMBOL_MINUS, tzStep, reinterpret_cast<void*>(intptr_t(-1)));
    tzVal_ = lv_label_create(tzCtl);
    lv_obj_add_style(tzVal_, Theme::body(), 0);
    smallButton(tzCtl, LV_SYMBOL_PLUS, tzStep, reinterpret_cast<void*>(intptr_t(1)));

    lv_obj_t* rb = lv_button_create(col);
    lv_obj_add_style(rb, Theme::button(), 0);
    lv_obj_set_width(rb, lv_pct(100));
    lv_obj_set_style_bg_color(rb, lv_color_hex(0x502020), 0);
    lv_obj_add_event_cb(rb, restoreClicked, LV_EVENT_CLICKED, nullptr);
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
    int32_t tz = SettingsStore::get(SettingKey::TzOff);
    int32_t atz = tz < 0 ? -tz : tz;
    lv_label_set_text_fmt(tzVal_, "UTC%c%02ld:%02ld", tz < 0 ? '-' : '+', (long)(atz / 60), (long)(atz % 60));

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
