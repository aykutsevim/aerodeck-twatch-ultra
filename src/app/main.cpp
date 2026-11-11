// AeroDeck firmware entry point: boot orchestration (research R3, data-model §6) and the UI loop.
#include <Arduino.h>
#include <cstring>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>
#include "board/IBoard.h"
#include "core/BootGuard.h"
#include "pages/HomePage.h"
#include "pages/PlaceholderPage.h"
#include "pages/SettingsPage.h"
#include "services/BoardCommands.h"
#include "services/Cautions.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/SvcTask.h"
#include "ui/CautionBanner.h"
#include "ui/DebugOverlay.h"
#include "ui/PageManager.h"
#include "ui/Theme.h"

using namespace core;

RTC_NOINIT_ATTR static BootRecord g_boot;

static bool g_ui;              // false in headless / PMU-missing mode
static const char* g_fatal;    // repeated over serial when !g_ui
static uint32_t g_lastFatalMs;
static bool g_stable;
static QueueHandle_t g_uiQueue;

static HomePage g_home;
static PlaceholderPage g_pageA(PageId::PlaceholderA, "PAGE A");
static PlaceholderPage g_pageB(PageId::PlaceholderB, "PAGE B");
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

static void applySetting(SettingKey k)
{
    switch (k) {
    case SettingKey::Bright:
        board().setBrightnessPct(uint8_t((SettingsStore::get(k) + 1) * 20));
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

static void onButton(void*) { PageManager::buttonPressed(); }

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

static void onFirstHome()
{
    LOGI("boot", "boot->home %lu ms", (unsigned long)(esp_timer_get_time() / 1000));
}

static void raisePeriphCautions()
{
    for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
        PeriphState s = board().periph(Periph(i));
        if (s == PeriphState::Failed || s == PeriphState::SimulatedFail) {
            char t[25];
            snprintf(t, sizeof t, "%s FAIL", periphName(Periph(i)));
            Cautions::raise(CautionId(uint8_t(CautionId::PeriphFirst) + i), t);
        }
    }
}

// FR-001: probe slow, S0-unused peripherals after Home is interactive (core 0, low priority).
static void deferredProbeTask(void*)
{
    uint32_t t0 = millis();
    board().probeDeferred();
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
        Cautions::raise(CautionId::SettingsReset, "SETTINGS RESET");
    } else if (st == SettingsStore::BootStatus::StorageError) {
        Cautions::raise(CautionId::SettingsStorageError, "SETTINGS STORAGE ERROR");
    }
    if (mode == BootMode::Safe) {
        Cautions::raise(CautionId::SafeMode, "SAFE MODE");
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
    SerialConsole::add("nav", 1, cmdNav);
    SvcTask::addTick(SettingsStore::tick);

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
    PowerService::begin();
    SvcTask::addTick(PowerService::tick);
    SvcTask::begin();

    g_uiQueue = EventBus::subscribe(16);
    PageManager::registerPage(&g_home);
    PageManager::registerPage(&g_pageA);
    PageManager::registerPage(&g_pageB);
    PageManager::registerPage(&g_settings);
    PageManager::onFirstHome(onFirstHome);

    CautionBanner::begin();
    DebugOverlay::begin();
    raiseBootCautions(br, st, mode);
    PageManager::begin();
    applySetting(SettingKey::Bright);
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
        case EventType::PeriphStatusChanged:
            g_settings.refresh();
            break;
        case EventType::CautionChanged:
            CautionBanner::refresh();
            g_settings.refresh();
            break;
        default:
            break;
        }
    }
    lv_timer_handler();
    delay(1);
}
