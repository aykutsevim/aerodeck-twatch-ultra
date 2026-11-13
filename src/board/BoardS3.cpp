// T-Watch S3 implementation of IBoard (wraps LilyGoLib `instance`). Compile-only target in S0.
#ifdef ARDUINO_T_WATCH_S3

#include <Arduino.h>
#include <Wire.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include "board/IBoard.h"
#include "board/I2cLock.h"
#include "board/I2cRecover.h"
#include "core/NvgColor.h"
#include "core/SimFail.h"
#include <esp_sleep.h>

using namespace core;

namespace {

constexpr uint8_t kPmuAddr = 0x34;

class BoardS3 : public IBoard {
public:
    void onStage(StageCb cb) override { stageCb_ = cb; }
    void probeDeferred() override {}   // S3 probes everything in begin()

    // BMA423 has no fusion output; the PFD shows ATT FAIL on the S3 (accelerometer fallback out of scope).
    bool configureAttitude(float, GravityCb, void*) override { return false; }

    bool readRtc(std::tm& utc, bool& integrity) override
    {
        if (periph(Periph::Rtc) != PeriphState::Ok) return false;
        I2cLock lock;
        utc = instance.rtc.getDateTime().toUnixTime();
        integrity = instance.rtc.isClockIntegrityGuaranteed();
        return true;
    }

    bool writeRtc(const std::tm& utc) override
    {
        if (periph(Periph::Rtc) != PeriphState::Ok) return false;
        I2cLock lock;
        instance.rtc.setDateTime(RTC_DateTime(utc));
        return true;
    }

    // S4: play a DRV2605 sequence (ROM effect ids; 0x80|n = wait n x 10 ms). Runs from any task (I2C lock).
    bool haptic(const uint8_t* seq, uint8_t n) override
    {
        if (periph(Periph::Haptics) != PeriphState::Ok || !seq || n == 0) {
            return false;
        }
        if (n > 7) {
            n = 7;   // 8 slots: the last one terminates the sequence
        }
        I2cLock lock;
        instance.drv.stop();
        for (uint8_t i = 0; i < n; i++) {
            instance.drv.setWaveform(i, seq[i]);
        }
        instance.drv.setWaveform(n, 0);
        instance.drv.run();
        return true;
    }

    // S6: the S3's BMA423 step counter is not wired in this compile-only target (research R10).
    bool configureMotion(float, MotionInfo& info) override
    {
        info = MotionInfo{false, false, 0.0f, 0.0f};
        return false;
    }
    bool stepCount(uint32_t&) override { return false; }
    bool takePeakG(float&, float&, float&) override { return false; }

    // S5: speaker via LilyGoLib's Player (I2S port PLAYER_IS2_PORT, installed by initAmplifier() at boot).
    bool audioBegin(uint32_t rateHz) override
    {
        if (periph(Periph::Speaker) != PeriphState::Ok || !audioReady_) {
            return false;
        }
        if (!instance.player.configureTX(rateHz, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO)) {
            return false;
        }
        i2s_zero_dma_buffer(PLAYER_IS2_PORT);
        return true;
    }

    size_t audioWrite(const int16_t* s, size_t n) override
    {
        if (!audioReady_ || !s || n == 0) {
            return 0;
        }
        return instance.player.write((void*)s, n * sizeof(int16_t)) / sizeof(int16_t);
    }

    void audioEnd() override
    {
        if (audioReady_) {
            i2s_zero_dma_buffer(PLAYER_IS2_PORT);
        }
    }

    void hapticStop() override
    {
        if (periph(Periph::Haptics) != PeriphState::Ok) {
            return;
        }
        I2cLock lock;
        instance.drv.stop();
    }

    BootResult begin(uint16_t simFailMask, bool safeMode, bool headless) override
    {
        BootResult r{true, safeMode, 0};
        simMask_ = safeMode ? 0xFF : simFailMask;

        stage(BootStage::PmuProbe);
        // Recover a bus left stuck by a soft reset mid-transfer, then probe (with retries).
        if (!i2c::probeWithRecovery(SDA, SCL, kPmuAddr)) {
            r.pmuOk = false;
            pmuOk_ = false;
            return r;
        }
        if (headless) {
            r.failedMask = 0xFFFF;
            return r;
        }

        uint32_t flags = NO_SCAN_I2C_DEV;
        if (SimFail::isSet(simMask_, Periph::Lora)) flags |= NO_HW_LORA;
        if (SimFail::isSet(simMask_, Periph::Gnss)) flags |= NO_HW_GPS;
        if (SimFail::isSet(simMask_, Periph::Mic)) flags |= NO_HW_MIC;
        if (SimFail::isSet(simMask_, Periph::Haptics)) flags |= NO_HW_DRV;
        if (SimFail::isSet(simMask_, Periph::Imu)) flags |= NO_HW_SENSOR;
        if (SimFail::isSet(simMask_, Periph::Rtc)) flags |= NO_HW_RTC;

        stage(BootStage::VendorInit);
        probe_ = instance.begin(flags);
        audioReady_ = !(flags & NO_HW_MIC);   // initAmplifier() ran (Player I2S installed)
        vendorOk_ = true;

        stage(BootStage::DisplayInit);
        beginLvglHelper(instance);
        wrapTouchRead();
        wrapFlush();
        lvgl_ = true;

        instance.onEvent(&BoardS3::onDeviceEvent, this, POWER_EVENT);
        for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
            PeriphState s = periph(Periph(i));
            if (s == PeriphState::Failed || s == PeriphState::SimulatedFail) {
                r.failedMask |= (1u << i);
            }
        }
        return r;
    }

    uint32_t caps() const override
    {
        return Cap::Lora | Cap::Haptics | Cap::Speaker | Cap::Mic | Cap::IrTx | Cap::ImuStepCounter;
    }
    const char* name() const override { return "T-Watch S3"; }
    DisplayInfo display() const override { return DisplayInfo{instance.width(), instance.height(), false, 0}; }

    void setBrightnessPct(uint8_t pct) override
    {
        if (pct > 100) pct = 100;
        brightness_ = uint8_t((uint32_t(pct) * DEVICE_MAX_BRIGHTNESS_LEVEL) / 100);
        if (displayOn_) {
            instance.setBrightness(brightness_);
        }
    }

    // S7: backlight off/on (LCD; no panel sleep needed for the S3 build).
    void setDisplayOn(bool on) override
    {
        if (on == displayOn_) return;
        displayOn_ = on;
        instance.setBrightness(on ? brightness_ : 0);
    }
    void setNvgFilter(bool on) override { g_nvgOn = on; }
    bool deepSleep(uint8_t, uint32_t) override { return false; }   // not validated on the S3 (contract)
    core::WakeCause wakeCause() const override
    {
        switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: return core::WakeCause::Button;
        case ESP_SLEEP_WAKEUP_TIMER: return core::WakeCause::Timer;
        case ESP_SLEEP_WAKEUP_UNDEFINED: return core::WakeCause::PowerOn;
        default: return core::WakeCause::Other;
        }
    }

    void setRail(Rail r, bool on) override
    {
        PowerCtrlChannel_t ch;
        switch (r) {
        case Rail::Lora: ch = POWER_RADIO; break;
        case Rail::AudioAmp: ch = POWER_SPEAK; break;
        case Rail::Gnss: ch = POWER_GPS; break;   // T-Watch S3 Plus only; harmless otherwise
        default: return;                          // no NFC; Display/Sensor never switched in S0
        }
        I2cLock lock;
        instance.powerControl(ch, on);
    }

    bool readPmu(PmuReading& out) override
    {
        if (!vendorOk_) return false;
        I2cLock lock;
        int pct = instance.pmu.getBatteryPercent();
        out.battPct = uint8_t(pct < 0 ? 0 : pct);
        out.battmV = instance.pmu.getBattVoltage();
        out.battmA = kCurrentUnavailable;
        out.dieTempC10 = int16_t(instance.pmu.getTemperature() * 10.0f);
        out.charging = instance.pmu.isCharging();
        out.vbus = instance.pmu.isVbusIn();
        return true;
    }

    void loop() override
    {
        if (vendorOk_) {
            I2cLock lock;
            instance.loop();
        }
    }

    void onButtonClick(ButtonCb cb, void* ctx) override
    {
        buttonCb_ = cb;
        buttonCtx_ = ctx;
    }

    PeriphState periph(Periph p) const override
    {
        auto has = [this](uint32_t m) { return (probe_ & m) != 0; };
        bool online;
        switch (p) {
        case Periph::Pmu: return pmuOk_ ? PeriphState::Ok : PeriphState::Failed;
        case Periph::Display: return lvgl_ ? PeriphState::Ok : PeriphState::Failed;
        case Periph::Touch: online = has(HW_TOUCH_ONLINE); break;
        case Periph::Rtc: online = has(HW_RTC_ONLINE); break;
        case Periph::Imu: online = has(HW_BMA423_ONLINE); break;
        case Periph::Haptics: online = has(HW_DRV_ONLINE); break;
        case Periph::Lora: online = has(HW_RADIO_ONLINE); break;
        case Periph::Gnss:
            // GNSS exists only on the T-Watch S3 Plus.
            if (has(HW_GPS_ONLINE)) return PeriphState::Ok;
            return SimFail::isSet(simMask_, p) ? PeriphState::SimulatedFail : PeriphState::Absent;
        case Periph::Mic:
        case Periph::Speaker:
            return SimFail::isSet(simMask_, Periph::Mic) ? PeriphState::SimulatedFail : PeriphState::Ok;
        case Periph::Nfc:
        case Periph::SdCard:
        case Periph::Expander:
        default:
            return PeriphState::Absent;
        }
        if (online) return PeriphState::Ok;
        return SimFail::isSet(simMask_, p) ? PeriphState::SimulatedFail : PeriphState::Failed;
    }

    bool lvglReady() const override { return lvgl_; }

private:
    // S7: our own flush callback, identical to LilyGoLib's disp_flush (LV_Helper_v9.cpp, incl. its _SWAP_COLORS
    // byte swap) plus the NVG filter applied before the swap.
    // (Reading the original callback needs LVGL's private display struct; that is avoided on purpose.)
    void wrapFlush()
    {
        lv_display_t* d = lv_display_get_default();
        if (d) {
            lv_display_set_flush_cb(d, &BoardS3::nvgFlush);
        }
    }
    static void nvgFlush(lv_display_t* d, const lv_area_t* area, uint8_t* px)
    {
        // LilyGoLib defines _SWAP_COLORS for this board (Ultra; S3 on IDF 4.x): the panel wants byte-swapped RGB565.
        // In NVG the recolour writes swapped pixels directly (one pass instead of two; research R1).
        if (g_nvgOn) {
            core::NvgColor::applySwapped(reinterpret_cast<uint16_t*>(px), size_t(lv_area_get_size(area)));
        } else {
            lv_draw_sw_rgb565_swap(px, lv_area_get_size(area));
        }
        auto* plane = static_cast<LilyGo_Display*>(lv_display_get_user_data(d));
        plane->pushColors(area->x1, area->y1, area->x2 + 1, area->y2 + 1, reinterpret_cast<uint16_t*>(px));
        lv_display_flush_ready(d);
    }
    static volatile bool g_nvgOn;
    bool displayOn_ = true;
    uint8_t brightness_ = DEVICE_MAX_BRIGHTNESS_LEVEL;

    void stage(BootStage s)
    {
        if (stageCb_) stageCb_(s);
    }

    static void onDeviceEvent(DeviceEvent_t event, void* params, void* user)
    {
        auto* self = static_cast<BoardS3*>(user);
        if (event == POWER_EVENT && instance.getPMUEventType(params) == PMU_EVENT_KEY_CLICKED && self->buttonCb_) {
            self->buttonCb_(self->buttonCtx_);
        }
    }

    void wrapTouchRead()
    {
        for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
            if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
                origTouchRead_ = lv_indev_get_read_cb(in);
                lv_indev_set_read_cb(in, &BoardS3::lockedTouchRead);
                return;
            }
        }
    }
    static void lockedTouchRead(lv_indev_t* in, lv_indev_data_t* data)
    {
        I2cLock lock;
        if (origTouchRead_) origTouchRead_(in, data);
    }

    static lv_indev_read_cb_t origTouchRead_;
    uint32_t probe_ = 0;
    uint16_t simMask_ = 0;
    bool pmuOk_ = true;
    bool vendorOk_ = false;
    bool lvgl_ = false;
    bool audioReady_ = false;   // S5: Player I2S driver installed
    StageCb stageCb_ = nullptr;
    ButtonCb buttonCb_ = nullptr;
    void* buttonCtx_ = nullptr;
};

lv_indev_read_cb_t BoardS3::origTouchRead_ = nullptr;
volatile bool BoardS3::g_nvgOn = false;
BoardS3 g_board;

} // namespace

IBoard& board() { return g_board; }

#endif // ARDUINO_T_WATCH_S3
