#include "ui/DebugOverlay.h"
#include <Arduino.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "core/SettingDefs.h"
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "ui/Theme.h"

namespace DebugOverlay {

static lv_obj_t* s_label;
static volatile uint32_t s_frames;
static volatile uint32_t s_total;
static Snapshot<DiagSnapshot> s_diag;

static void onRenderReady(lv_event_t*)
{
    s_frames++;
    s_total++;
}

uint32_t frameCount() { return s_total; }

static void sampleCb(lv_timer_t*)
{
    DiagSnapshot d;
    uint32_t f = s_frames;
    s_frames = 0;
    d.fps = uint16_t(f);
    d.heapFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    d.heapMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    d.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s_diag.write(d);
    if (s_label && !lv_obj_has_flag(s_label, LV_OBJ_FLAG_HIDDEN)) {
        lv_label_set_text_fmt(s_label, "%u fps  heap %luK (min %luK)  psram %luK", d.fps,
                              (unsigned long)(d.heapFree / 1024), (unsigned long)(d.heapMin / 1024),
                              (unsigned long)(d.psramFree / 1024));
    }
}

void begin()
{
    lv_display_add_event_cb(lv_display_get_default(), onRenderReady, LV_EVENT_RENDER_READY, nullptr);
    s_label = lv_label_create(lv_layer_top());
    lv_obj_set_style_bg_color(s_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_label, LV_OPA_60, 0);
    lv_obj_set_style_text_color(s_label, Theme::hudGreen(), 0);
    lv_obj_set_style_text_font(s_label, Theme::fontSmall(), 0);
    lv_obj_set_style_pad_all(s_label, Theme::S(2), 0);
    lv_obj_align(s_label, LV_ALIGN_BOTTOM_LEFT, Theme::S(2), -Theme::S(2));
    lv_obj_remove_flag(s_label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(s_label, "-- fps");
    lv_timer_create(sampleCb, 1000, nullptr);
    setVisible(SettingsStore::getBool(core::SettingKey::Overlay));
}

void setVisible(bool on)
{
    if (!s_label) {
        return;
    }
    if (on) {
        lv_obj_remove_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    }
}

const Snapshot<DiagSnapshot>& diag() { return s_diag; }

static void cmdStat(int, const char* const*, core::Reply& r)
{
    DiagSnapshot d = s_diag.read();
    char b[96];
    snprintf(b, sizeof b, "fps=%u heap=%lu heapmin=%lu psram=%lu", d.fps, (unsigned long)d.heapFree,
             (unsigned long)d.heapMin, (unsigned long)d.psramFree);
    r.ok();
    r.line(b);
}

static void cmdLog(int, const char* const* argv, core::Reply& r)
{
    static const char* names[] = {"error", "warn", "info", "debug"};
    for (int i = 0; i < 4; i++) {
        if (strcmp(argv[0], names[i]) == 0) {
            SettingsStore::set(core::SettingKey::LogLvl, i);   // persisted; applied via SettingsChanged
            Log::setLevel(core::Lvl(i));
            r.ok();
            return;
        }
    }
    r.err("expected error|warn|info|debug");
}

void registerCommands()
{
    SerialConsole::add("stat", 0, cmdStat);
    SerialConsole::add("log", 1, cmdLog);
}

} // namespace DebugOverlay
