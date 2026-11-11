#include "ui/PageManager.h"
#include <Arduino.h>
#include "core/NavModel.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "ui/DebugOverlay.h"
#include "ui/PageIndicator.h"
#include "ui/Theme.h"

namespace PageManager {

static Page* s_pages[kMaxPages];
static lv_obj_t* s_screens[kMaxPages];
static int s_n;
static core::NavModel* s_nav;
static int s_from;
static uint32_t s_navStartMs;
static uint32_t s_navStartFrames;
static const char* s_navKind = "swipe";
static void (*s_firstHomeCb)();
static bool s_firstHomeDone;
static uint32_t s_lastTickMs;

static void navigate(int target, lv_screen_load_anim_t anim, const char* kind);

static void onGesture(lv_event_t*)
{
    lv_indev_t* in = lv_indev_active();
    if (!in) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(in);
    if (dir == LV_DIR_LEFT) {
        next();
        lv_indev_wait_release(in);
    } else if (dir == LV_DIR_RIGHT) {
        prev();
        lv_indev_wait_release(in);
    }
    // TOP/BOTTOM gestures are ignored (edge case: vertical swipes do not change pages).
}

static void onLoaded(lv_event_t*)
{
    int to = s_nav->current();
    (void)to;
    int queued = s_nav->transitionDone();
    int cur = s_nav->current();
    uint32_t dt = millis() - s_navStartMs;
    uint32_t frames = DebugOverlay::frameCount() - s_navStartFrames;
    LOGI("nav", "nav %s->%s %s %lu ms %lu frames %lu fps", s_pages[s_from]->title(), s_pages[cur]->title(),
         s_navKind, (unsigned long)dt, (unsigned long)frames, (unsigned long)(dt ? frames * 1000 / dt : 0));
    s_pages[cur]->onEnter();
    EventBus::publish(EventType::PageChanged, uint32_t(s_pages[s_from]->id()), uint32_t(s_pages[cur]->id()));
    PageIndicator::show(cur, s_n, s_pages[cur]->title());
    if (cur == 0 && !s_firstHomeDone) {
        s_firstHomeDone = true;
        if (s_firstHomeCb) {
            s_firstHomeCb();
        }
    }
    if (queued >= 0) {
        navigate(queued, LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM, "button");
    }
}

static lv_obj_t* screenFor(int i)
{
    if (!s_screens[i]) {
        lv_obj_t* scr = lv_obj_create(nullptr);
        Theme::styleScreen(scr);
        lv_obj_add_event_cb(scr, onGesture, LV_EVENT_GESTURE, nullptr);
        lv_obj_add_event_cb(scr, onLoaded, LV_EVENT_SCREEN_LOADED, nullptr);
        s_pages[i]->onCreate(scr);
        s_screens[i] = scr;
    }
    return s_screens[i];
}

static void navigate(int target, lv_screen_load_anim_t anim, const char* kind)
{
    if (target < 0 || target >= s_n) {
        return;
    }
    s_from = s_nav->current();
    if (target == s_from) {
        return;
    }
    s_navStartMs = millis();
    s_navStartFrames = DebugOverlay::frameCount();
    s_navKind = kind;
    s_pages[s_from]->onExit();
    s_nav->transitionStarted(target);
    lv_screen_load_anim(screenFor(target), anim, 200, 0, false);
}

static void tickCb(lv_timer_t*)
{
    uint32_t now = millis();
    uint32_t dt = now - s_lastTickMs;
    s_lastTickMs = now;
    if (s_nav && !s_nav->busy()) {
        s_pages[s_nav->current()]->onTick(dt);
    }
}

void registerPage(Page* p)
{
    if (p && s_n < kMaxPages) {
        s_pages[s_n++] = p;
    }
}

void begin()
{
    static core::NavModel nav(s_n);
    s_nav = &nav;
    PageIndicator::begin();
    s_from = 0;
    // LilyGoLib's lv_conf.h sets LV_DEF_REFR_PERIOD 33 ms (≤30 fps cap); allow up to ~60 fps (SC-006).
    lv_timer_set_period(lv_display_get_refr_timer(lv_display_get_default()), 16);
    s_navStartMs = millis();
    s_navKind = "boot";
    s_nav->transitionStarted(0);
    lv_screen_load(screenFor(0));   // fires LV_EVENT_SCREEN_LOADED → onEnter + indicator
    s_lastTickMs = millis();
    lv_timer_create(tickCb, 100, nullptr);   // onTick at 10 Hz
}

void next() { navigate(s_nav->next(), LV_SCREEN_LOAD_ANIM_MOVE_LEFT, "swipe"); }
void prev() { navigate(s_nav->prev(), LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, "swipe"); }

void buttonPressed()
{
    int t = s_nav->button();
    if (t >= 0) {
        navigate(t, t == 0 ? LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM : LV_SCREEN_LOAD_ANIM_MOVE_TOP, "button");
    }
}

PageId current() { return s_nav ? s_pages[s_nav->current()]->id() : PageId::Home; }
int count() { return s_n; }
Page* page(int index) { return (index >= 0 && index < s_n) ? s_pages[index] : nullptr; }
void onFirstHome(void (*cb)()) { s_firstHomeCb = cb; }

} // namespace PageManager
