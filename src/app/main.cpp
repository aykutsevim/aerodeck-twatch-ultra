// AeroDeck firmware entry point: boot orchestration (research R3, data-model §6) and the UI loop.
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>
#include "board/IBoard.h"
#include "core/BootGuard.h"
#include "pages/PfdPage.h"
#include "pages/RecorderPage.h"
#include "pages/SettingsPage.h"
#include "pages/WxPage.h"
#include "services/AttitudeService.h"
#include "services/AudioService.h"
#include "services/BoardCommands.h"
#include "services/NetService.h"
#include "services/AlertManager.h"
#include "services/HapticService.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/RecorderService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/SvcTask.h"
#include "services/TimeService.h"
#include "services/WeatherService.h"
#include "ui/AnnunciatorBar.h"
#include "ui/DebugOverlay.h"
#include "ui/NetSetupOverlay.h"
#include "ui/PageManager.h"
#include "ui/ScreenService.h"
#include "ui/Theme.h"

using namespace core;

RTC_NOINIT_ATTR static BootRecord g_boot;

static bool g_ui;              // false in headless / PMU-missing mode
static const char* g_fatal;    // repeated over serial when !g_ui
static uint32_t g_lastFatalMs;
static bool g_stable;
static QueueHandle_t g_uiQueue;

static PfdPage g_home;
static WxPage g_wx;   // S3: replaces the "PAGE A" placeholder
static RecorderPage g_recorder;   // S6: replaces the "PAGE B" placeholder
static SettingsPage g_settings;

static bool abnormalReset()
{
    switch (esp_reset_reason()) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
        return true;
    default:
        return false;
    }
}

static void onStage(BootStage s) { BootGuard::mark(g_boot, s); }

// S7: NVG forces 10 % (FR-013/016); otherwise the stored level (20..100 %).
static void applyBrightness()
{
    const bool nvg = SettingsStore::getBool(SettingKey::Nvg);
    const uint8_t pct = nvg ? 10 : uint8_t((SettingsStore::get(SettingKey::Bright) + 1) * 20);
    board().setBrightnessPct(pct);
    ScreenService::setApplied(nvg, pct);
}

static void applySetting(SettingKey k)
{
    switch (k) {
    case SettingKey::Bright:
        applyBrightness();
        break;
    case SettingKey::Nvg: {   // S7 (FR-013/014): flush filter + brightness, from the first frame
        const bool on = SettingsStore::getBool(k);
        board().setNvgFilter(on);
        AnnunciatorBar::setNvg(on);
        applyBrightness();
        lv_obj_invalidate(lv_screen_active());
        lv_obj_invalidate(lv_layer_top());
        EventBus::publish(EventType::NvgChanged, on ? 1u : 0u);
        LOGI("nvg", "%s", on ? "on" : "off");
        break;
    }
    case SettingKey::AttRef:
        AttitudeService::setReferenceTilt(uint8_t(SettingsStore::get(k)));
        break;
    case SettingKey::ScrTo:
    case SettingKey::Raise:
        ScreenService::configure(uint8_t(SettingsStore::get(SettingKey::ScrTo)),
                                 SettingsStore::getBool(SettingKey::Raise));
        break;
    case SettingKey::LogLvl:
        Log::setLevel(Lvl(SettingsStore::get(k)));
        break;
    case SettingKey::Overlay:
        DebugOverlay::setVisible(SettingsStore::getBool(k));
        break;
    default:
        break;
    }
}

static void onButton(void*)
{
    if (!ScreenService::onButton()) {   // S7: a press that wakes the screen does nothing else (FR-002)
        PageManager::buttonPressed();
    }
}

// Test hook (quickstart V3 automation): `ad nav next|prev|button` → UI task via EventBus.
static void cmdNav(int, const char* const* argv, Reply& r)
{
    uint32_t a = strcmp(argv[0], "next") == 0 ? 0 : strcmp(argv[0], "prev") == 0 ? 1
               : strcmp(argv[0], "button") == 0 ? 2 : 99;
    if (a == 99) {
        r.err("expected next|prev|button");
        return;
    }
    EventBus::publish(EventType::NavCommand, a);
    r.ok();
}

static uint32_t g_bootHomeMs;   // S7: boot (incl. wake from deep sleep) → first watch-face frame, for `ad boot`

static void onFirstHome()
{
    g_bootHomeMs = uint32_t(esp_timer_get_time() / 1000);
    LOGI("boot", "boot->home %lu ms", (unsigned long)g_bootHomeMs);
    PowerService::enableBatteryAlarm();   // S4: STALL / PULL UP only once the face (and the band) is up
}

static void raisePeriphCautions()
{
    for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
        PeriphState s = board().periph(Periph(i));
        if (s == PeriphState::Failed || s == PeriphState::SimulatedFail) {
            AlertManager::raise(core::AlertId(uint8_t(core::AlertId::PeriphFirst) + i));   // "<NAME> FAIL"
        }
    }
}

// FR-001: probe slow, S0-unused peripherals after Home is interactive (core 0, low priority).
static void deferredProbeTask(void*)
{
    uint32_t t0 = millis();
    board().probeDeferred();
    AttitudeService::onImuReady(board().periph(Periph::Imu) == PeriphState::Ok);
    RecorderService::onImuReady(board().periph(Periph::Imu) == PeriphState::Ok);   // S6: hub is up now
    PowerService::enforceIdleRails();
    raisePeriphCautions();
    EventBus::publish(EventType::PeriphStatusChanged);
    LOGI("boot", "deferred probe done in %lu ms", (unsigned long)(millis() - t0));
    vTaskDelete(nullptr);
}

static void raiseBootCautions(const BootResult& br, SettingsStore::BootStatus st, BootMode mode)
{
    raisePeriphCautions();   // Pending peripherals are reported after the deferred probe
    if (st == SettingsStore::BootStatus::ResetNewer) {
        AlertManager::raise(core::AlertId::SettingsReset);
    } else if (st == SettingsStore::BootStatus::StorageError) {
        AlertManager::raise(core::AlertId::SettingsStorageError);
    }
    if (mode == BootMode::Safe) {
        AlertManager::raise(core::AlertId::SafeMode);
    }
    (void)br;
}

void setup()
{
    Serial.begin(115200);
    // USB-CDC is opened by the core before setup(); a short TX timeout lets bursts of log/reply lines
    // wait for the host instead of being dropped (the HWCDC skips the wait when no host is connected).
    Serial.setTxTimeoutMs(50);
    Log::begin();

    BootMode mode = BootGuard::onBoot(g_boot, abnormalReset());
    SettingsStore::BootStatus st = SettingsStore::begin();
    Log::setLevel(Lvl(SettingsStore::get(SettingKey::LogLvl)));
    LOGI("boot", "AeroDeck starting; mode=%d fastFails=%u settings=%d", int(mode), g_boot.fastFailCount,
         int(st));

    SerialConsole::begin();
    SerialConsole::setSafeModeFlag(mode != BootMode::Normal);
    SettingsStore::registerCommands();
    BoardCommands::registerCommands();
    DebugOverlay::registerCommands();
    PowerService::registerCommands();
    SerialConsole::add("nav", 1, cmdNav);
    SerialConsole::add("boot", 0, [](int, const char* const*, Reply& r) {   // S7 (FR-011): wake → PFD time
        char line[64];
        snprintf(line, sizeof line, "boot_to_home_ms=%lu wake=%s", (unsigned long)g_bootHomeMs,
                 core::wakeCauseName(board().wakeCause()));
        r.ok();
        r.line(line);
    });
    ScreenService::registerCommands();
    SvcTask::addTick(SettingsStore::tick);
    AlertManager::begin();     // S4: before anything raises an alert (TimeService, boot cautions)
    HapticService::begin();
    SvcTask::addTick(AlertManager::tick);

#ifdef AD_TEST_DISPLAY_CRASH
    // T047 verification only: simulate a crash during display init.
    BootGuard::mark(g_boot, BootStage::DisplayInit);
    if (mode != BootMode::Headless) {
        abort();
    }
#endif

    board().onStage(onStage);
    uint16_t simMask = uint16_t(SettingsStore::get(SettingKey::SimFail));
    BootResult br = board().begin(simMask, mode == BootMode::Safe, mode == BootMode::Headless);

    if (!br.pmuOk) {
        g_fatal = "FATAL: PMU (AXP2101 @0x34) not found; staying idle. Use 'ad info' / 'ad reboot'";
    } else if (mode == BootMode::Headless) {
        g_fatal = "FATAL: display init failed repeatedly; headless mode. Use 'ad reboot' or 'ad set reset'";
    }
    if (g_fatal) {
        SvcTask::begin();   // serial console only
        LOGE("boot", "%s", g_fatal);
        return;
    }

    g_ui = true;
    BootGuard::mark(g_boot, BootStage::UiInit);
    Theme::begin(board().display());
    TimeService::begin();
    AttitudeService::begin();
    PowerService::begin();
    SvcTask::addTick(PowerService::tick);
    AudioService::begin();               // S5: after PowerService (AudioAmp lease) and TimeService
    SvcTask::addTick(AudioService::tick);
    SvcTask::addTick(TimeService::tick);
    RecorderService::begin();            // S6: loads the saved days; motion starts in onImuReady
    SvcTask::addTick(RecorderService::tick);
    SvcTask::addTick(AttitudeService::tick);
    WeatherService::begin();             // S3: before NetService (TLS allocator before Wi-Fi is first used)
    SvcTask::addTick(WeatherService::tick);
    NetService::begin();                 // after PowerService (Rail::Wifi hook) and TimeService
    SvcTask::addTick(NetService::tick);
    SvcTask::begin();

    g_uiQueue = EventBus::subscribe(16);
    PageManager::registerPage(&g_home);
    PageManager::registerPage(&g_wx);
    PageManager::registerPage(&g_recorder);
    PageManager::registerPage(&g_settings);
    PageManager::onFirstHome(onFirstHome);

    AnnunciatorBar::begin();   // S4: replaces the S0 caution banner
    DebugOverlay::begin();
    NetSetupOverlay::begin();
    raiseBootCautions(br, st, mode);
    PageManager::begin();
    ScreenService::begin();
    applySetting(SettingKey::Nvg);       // S7: NVG + brightness before the first frame (FR-014)
    applySetting(SettingKey::ScrTo);
    applySetting(SettingKey::AttRef);
    LOGI("boot", "wake=%s", core::wakeCauseName(board().wakeCause()));
    board().onButtonClick(onButton, nullptr);
    enableLoopWDT();   // FR-016: a hung UI loop resets the chip

    LOGI("boot", "ui ready; failedMask=0x%04x", br.failedMask);
    xTaskCreatePinnedToCore(deferredProbeTask, "probe", 6144, nullptr, 1, nullptr, 0);
}

void loop()
{
    uint32_t now = millis();
    if (!g_stable && now >= BootGuard::kStableMs) {
        g_stable = true;
        BootGuard::onStable(g_boot);
        LOGD("boot", "stable");
    }
    if (!g_ui) {
        if (now - g_lastFatalMs >= 5000) {
            g_lastFatalMs = now;
            LOGE("boot", "%s", g_fatal ? g_fatal : "FATAL");
        }
        delay(50);
        return;
    }

    // S7 eject quiesce (research R6): stop LVGL and the panel, hand the bus to the svc task, then idle.
    static bool s_quiet;
    if (PowerService::ejecting()) {
        if (!s_quiet) {
            s_quiet = true;
            ScreenService::forceOff();   // waits for the last transfer, panel off
            PowerService::uiQuiesced();
        }
        delay(10);
        return;
    }
    if (s_quiet) {   // deep sleep was unsupported: resume
        s_quiet = false;
        ScreenService::command(1);
    }
    board().loop();
    Event e;
    while (xQueueReceive(g_uiQueue, &e, 0) == pdTRUE) {
        switch (e.type) {
        case EventType::SettingsChanged:
            applySetting(SettingKey(e.a));
            g_settings.refresh();
            break;
        case EventType::NavCommand:
            if (e.a == 0) PageManager::next();
            else if (e.a == 1) PageManager::prev();
            else PageManager::buttonPressed();
            break;
        case EventType::NetStatusChanged:
            NetSetupOverlay::update(NetService::status().read());
            g_settings.refresh();
            break;
        case EventType::PeriphStatusChanged:
            g_settings.refresh();
            break;
        case EventType::AlertRaised:
            if (e.b != 0) {   // S7: only new/re-armed, unacknowledged alerts may wake the screen (FR-004)
                ScreenService::onAlert(core::AlertLevel(e.b));
            }
            AnnunciatorBar::update();
            break;
        case EventType::ScreenCommand:
            ScreenService::command(uint8_t(e.a));
            break;
        case EventType::AlertCleared:
        case EventType::AlertAcked:
            AnnunciatorBar::update();
            break;
        default:
            break;
        }
    }
    AnnunciatorBar::update();   // cheap seq check (alerts raised before the event queue existed)
    ScreenService::tick(now);
    if (ScreenService::isOn()) {
        lv_timer_handler();
        delay(1);
    } else {
        delay(10);   // S7: screen off: no rendering, no touch reads; button/gravity still pumped above
    }
}
