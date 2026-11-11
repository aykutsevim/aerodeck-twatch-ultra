// Minimal "is it alive?" app for LilyGo T-Watch-Ultra
#include <LilyGoLib.h>
#include <LV_Helper.h>

static lv_obj_t *uptime_label;
static uint32_t taps = 0;

static void btn_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_obj_get_child(lv_event_get_target_obj(e), 0);
    lv_label_set_text_fmt(label, "Tapped %lu", (unsigned long)++taps);
    Serial.printf("Tap %lu\n", (unsigned long)taps);
    instance.vibrator();   // short buzz as extra proof
}

static void tick_cb(lv_timer_t *)
{
    lv_label_set_text_fmt(uptime_label, "Uptime: %lu s", (unsigned long)(millis() / 1000));
}

void setup()
{
    Serial.begin(115200);
    instance.begin();
    beginLvglHelper(instance);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003366), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Hello, T-Watch Ultra!");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

    uptime_label = lv_label_create(scr);
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0x00FF88), 0);
    lv_obj_align(uptime_label, LV_ALIGN_CENTER, 0, -20);
    tick_cb(nullptr);
    lv_timer_create(tick_cb, 1000, nullptr);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 180, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Tap me");
    lv_obj_center(bl);

    instance.setBrightness(DEVICE_MAX_BRIGHTNESS_LEVEL);
    Serial.println("T-Watch Ultra hello app started");
}

void loop()
{
    lv_timer_handler();
    delay(2);
}
