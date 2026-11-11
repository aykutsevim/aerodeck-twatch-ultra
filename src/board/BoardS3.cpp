// T-Watch S3 implementation of IBoard (wraps LilyGoLib `instance`). Compile-only target in S0.
#ifdef ARDUINO_T_WATCH_S3

#include <Arduino.h>
#include <Wire.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include "board/IBoard.h"
#include "board/I2cLock.h"
#include "board/I2cRecover.h"
#include "core/SimFail.h"

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
        vendorOk_ = true;

        stage(BootStage::DisplayInit);
        beginLvglHelper(instance);
        wrapTouchRead();
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
        instance.setBrightness(uint8_t((uint32_t(pct) * DEVICE_MAX_BRIGHTNESS_LEVEL) / 100));
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
    StageCb stageCb_ = nullptr;
    ButtonCb buttonCb_ = nullptr;
    void* buttonCtx_ = nullptr;
};

lv_indev_read_cb_t BoardS3::origTouchRead_ = nullptr;
BoardS3 g_board;

} // namespace

IBoard& board() { return g_board; }

#endif // ARDUINO_T_WATCH_S3
