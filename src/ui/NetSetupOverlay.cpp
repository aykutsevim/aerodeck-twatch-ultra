#include "ui/NetSetupOverlay.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include "ui/Theme.h"

namespace NetSetupOverlay {

static lv_obj_t* s_root;
static lv_obj_t* s_qr;
static lv_obj_t* s_ssid;
static lv_obj_t* s_code;
static lv_obj_t* s_timer;
static char s_shownSsid[16];
static char s_shownCode[9];

static void cancelCb(lv_event_t*) { NetService::stopSetup(); }

static lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

void begin()
{
    s_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_root, Theme::S(4), 0);
    // Content stays well inside the 64 px rounded corners: centred column, side padding.
    lv_obj_set_style_pad_hor(s_root, Theme::S(16), 0);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);   // swallow touches behind the overlay

    lv_obj_t* t = label(s_root, Theme::fontTitle(), Theme::hudGreen());
    lv_label_set_text(t, "WI-FI SETUP");
    lv_obj_t* h = label(s_root, Theme::fontSmall(), Theme::dim());
    lv_label_set_text(h, "Scan with your phone camera, or join:");

    s_qr = lv_qrcode_create(s_root);
    lv_qrcode_set_size(s_qr, Theme::S(100));
    lv_qrcode_set_dark_color(s_qr, lv_color_black());
    lv_qrcode_set_light_color(s_qr, lv_color_white());
    lv_obj_set_style_border_color(s_qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(s_qr, Theme::S(4), 0);

    s_ssid = label(s_root, Theme::fontBody(), Theme::fg());
    s_code = label(s_root, Theme::fontTitle(), Theme::caution());
    lv_obj_t* url = label(s_root, Theme::fontSmall(), Theme::dim());
    lv_label_set_text(url, "then open http://192.168.4.1");
    s_timer = label(s_root, Theme::fontBody(), Theme::fg());

    lv_obj_t* b = lv_button_create(s_root);
    lv_obj_add_style(b, Theme::button(), 0);
    lv_obj_set_width(b, Theme::S(120));
    lv_obj_add_event_cb(b, cancelCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(b);
    lv_label_set_text(bl, "Cancel");
    lv_obj_center(bl);

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

void update(const NetStatus& st)
{
    if (!s_root) {
        return;
    }
    const bool active = st.busy == NetBusy::Setup && st.setupSsid[0] != '\0';
    if (!active) {
        if (!lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
            s_shownSsid[0] = '\0';
            memset(s_shownCode, 0, sizeof s_shownCode);
            lv_label_set_text(s_code, "");
        }
        return;
    }
    if (strcmp(s_shownSsid, st.setupSsid) != 0 || strcmp(s_shownCode, st.setupCode) != 0) {
        strncpy(s_shownSsid, st.setupSsid, sizeof s_shownSsid - 1);
        strncpy(s_shownCode, st.setupCode, sizeof s_shownCode - 1);
        char qr[64];
        snprintf(qr, sizeof qr, "WIFI:T:WPA;S:%s;P:%s;;", st.setupSsid, st.setupCode);
        lv_qrcode_update(s_qr, qr, uint32_t(strlen(qr)));
        memset(qr, 0, sizeof qr);
        lv_label_set_text(s_ssid, st.setupSsid);
        lv_label_set_text_fmt(s_code, "Code: %s", st.setupCode);
    }
    lv_label_set_text_fmt(s_timer, "Closes in %u:%02u", st.setupRemainingS / 60, st.setupRemainingS % 60);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

} // namespace NetSetupOverlay
