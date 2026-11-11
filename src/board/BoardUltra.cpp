// T-Watch Ultra implementation of IBoard (wraps LilyGoLib `instance`).
#ifdef ARDUINO_T_WATCH_S3_ULTRA

#include <Arduino.h>
#include <Wire.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include "board/IBoard.h"
#include "board/I2cLock.h"
#include "board/I2cRecover.h"
#include "core/SimFail.h"
#include <bosch/bhy2_parse.h>

using namespace core;

namespace {

constexpr uint8_t kPmuAddr = 0x34;

struct Deferred {
    Periph p;
    uint32_t noHwFlag;
    uint32_t onlineBit;
};
const Deferred kDeferred[] = {
    {Periph::Imu, NO_HW_SENSOR, HW_BHI260AP_ONLINE}, {Periph::Nfc, NO_HW_NFC, HW_NFC_ONLINE},
    {Periph::Gnss, NO_HW_GPS, HW_GPS_ONLINE},         {Periph::Lora, NO_HW_LORA, HW_RADIO_ONLINE},
    {Periph::SdCard, NO_HW_SD, HW_SD_ONLINE},
};

class BoardUltra : public IBoard {
public:
    BootResult begin(uint16_t simFailMask, bool safeMode, bool headless) override
    {
        BootResult r{true, safeMode, 0};
        simMask_ = simFailMask;

        // R3: LilyGoLib asserts if the PMU is missing; probe first so we never enter that path.
        stage(BootStage::PmuProbe);
        // Recover a bus left stuck by a soft reset mid-transfer, then probe (with retries).
        if (!i2c::probeWithRecovery(SDA, SCL, kPmuAddr)) {
            r.pmuOk = false;
            pmuOk_ = false;
            return r;
        }

        if (headless) {
            // LilyGoLib initialises the display panel inside instance.begin(), so headless must skip
            // vendor init entirely: only the serial console runs (BootGuard decided display init crashes).
            vendorOk_ = false;
            r.failedMask = 0xFFFF;
            return r;
        }

        uint32_t flags = NO_SCAN_I2C_DEV;
        if (safeMode) {
            simMask_ = 0xFF;   // all optional peripherals skipped
        }
        flags |= noHwFlags(simMask_);
        // FR-001: slow peripherals not used in S0 are probed after Home is shown (probeDeferred).
        for (const Deferred& d : kDeferred) {
            if (!SimFail::isSet(simMask_, d.p)) {
                flags |= d.noHwFlag;
                pending_ |= (1u << uint8_t(d.p));
            }
        }

        stage(BootStage::VendorInit);   // note: LilyGoLib also brings up the panel in here
        instance.setDisplayParams(true, false);   // DMA partial refresh: measured fastest (validation.md, R7)
        probe_ = instance.begin(flags);
        vendorOk_ = true;

        stage(BootStage::DisplayInit);
        beginLvglHelper(instance);
        wrapTouchRead();
        lvgl_ = true;
        displayOk_ = true;

        instance.onEvent(&BoardUltra::onDeviceEvent, this, POWER_EVENT);

        for (uint8_t i = 0; i < uint8_t(Periph::Count); i++) {
            PeriphState s = periph(Periph(i));
            if (s == PeriphState::Failed || s == PeriphState::SimulatedFail) {
                r.failedMask |= (1u << i);
            }
        }
        return r;
    }

    void onStage(StageCb cb) override { stageCb_ = cb; }

    bool configureAttitude(float rateHz, GravityCb cb, void* ctx) override
    {
        if (periph(Periph::Imu) != PeriphState::Ok) {
            return false;
        }
        gravityCb_ = cb;
        gravityCtx_ = ctx;
        I2cLock lock;
        gravityScale_ = instance.sensor.getScaling(BHY2_SENSOR_ID_GRA);
        if (!(gravityScale_ > 0.0f)) {
            gravityScale_ = 1.0f / 4096.0f;   // S1 only needs axis ratios; S6 must confirm the absolute scale
        }
        bool ok = instance.sensor.onResultEvent(SensorBHI260AP::GRAVITY_VECTOR, &BoardUltra::onGravity, this);
        ok = ok && instance.sensor.configure(BHY2_SENSOR_ID_GRA, rateHz, 0);
        return ok;
    }

    bool readRtc(std::tm& utc, bool& integrity) override
    {
        if (periph(Periph::Rtc) != PeriphState::Ok) {
            return false;
        }
        I2cLock lock;
        RTC_DateTime d = instance.rtc.getDateTime();
        utc = d.toUnixTime();
        integrity = instance.rtc.isClockIntegrityGuaranteed();
        return true;
    }

    bool writeRtc(const std::tm& utc) override
    {
        if (periph(Periph::Rtc) != PeriphState::Ok) {
            return false;
        }
        I2cLock lock;
        instance.rtc.setDateTime(RTC_DateTime(utc));
        return true;
    }

    void probeDeferred() override
    {
        for (const Deferred& d : kDeferred) {
            if (!(pending_ & (1u << uint8_t(d.p)))) {
                continue;
            }
            bool ok;
            {
                I2cLock lock;   // BHI260AP/GPS power go over I2C; SPI devices use LilyGoLib's own SPI lock
                // PowerService switched the optional rails off at boot; LilyGoLib only powers them in
                // initPMU(), so power the rail briefly for the probe. enforceIdleRails() turns it off after.
                switch (d.p) {
                case Periph::Nfc: instance.powerControl(POWER_NFC, true); break;
                case Periph::Gnss: instance.powerControl(POWER_GPS, true); break;   // also starts Serial1
                case Periph::Lora: instance.powerControl(POWER_RADIO, true); break;
                default: break;
                }
                delay(20);
                switch (d.p) {
                case Periph::Imu: ok = instance.initSensor(); break;
                case Periph::Nfc: ok = instance.initNFC(); break;
                case Periph::Gnss: ok = instance.initGPS(); break;
                case Periph::Lora: ok = instance.initLoRa(); break;
                case Periph::SdCard:
                    // Card-detect via the expander: HIGH = no card inserted → ABSENT, not FAILED.
                    instance.io.pinMode(EXPANDS_SD_DET, INPUT);
                    if (instance.io.digitalRead(EXPANDS_SD_DET) == HIGH) {
                        absent_ |= (1u << uint8_t(Periph::SdCard));
                        ok = false;
                    } else {
                        ok = instance.installSD();
                    }
                    break;
                default: ok = false; break;
                }
            }
            if (ok) {
                probe_ |= d.onlineBit;
            }
            pending_ &= ~(1u << uint8_t(d.p));
            vTaskDelay(1);
        }
    }

    uint32_t caps() const override
    {
        return Cap::Lora | Cap::Haptics | Cap::Speaker | Cap::Mic | Cap::Nfc | Cap::Gnss | Cap::SdCard |
               Cap::ImuStepCounter | Cap::Amoled;
    }

    const char* name() const override { return "T-Watch Ultra"; }

    DisplayInfo display() const override
    {
        // Rotated size as LVGL sees it. DISP_WIDTH/DISP_HEIGHT (502×410) describe the panel before rotation;
        // LilyGoLib's default rotation 0 is portrait, i.e. 410 wide × 502 tall.
        // The Ultra AMOLED has rounded corners with a 64 px radius.
        return DisplayInfo{instance.width(), instance.height(), false, 64};
    }

    void setBrightnessPct(uint8_t pct) override
    {
        if (pct > 100) {
            pct = 100;
        }
        I2cLock lock;
        instance.setBrightness(uint8_t((uint32_t(pct) * DEVICE_MAX_BRIGHTNESS_LEVEL) / 100));
    }

    void setRail(Rail r, bool on) override
    {
        PowerCtrlChannel ch;
        switch (r) {
        case Rail::Lora: ch = POWER_RADIO; break;
        case Rail::AudioAmp: ch = POWER_SPEAK; break;
        case Rail::Gnss: ch = POWER_GPS; break;
        case Rail::Nfc: ch = POWER_NFC; break;
        // Display (ALDO2, also powers touch) and Sensor (ALDO4) are never switched off in S0.
        case Rail::Display:
        case Rail::Sensor:
        default:
            return;
        }
        I2cLock lock;
        instance.powerControl(ch, on);
    }

    bool readPmu(PmuReading& out) override
    {
        if (!vendorOk_) {
            return false;
        }
        I2cLock lock;
        out.battPct = uint8_t(instance.pmu.getBatteryPercent() < 0 ? 0 : instance.pmu.getBatteryPercent());
        out.battmV = instance.pmu.getBattVoltage();
        out.battmA = kCurrentUnavailable;   // AXP2101 has no battery current ADC
        out.dieTempC10 = int16_t(instance.pmu.getTemperature() * 10.0f);
        out.charging = instance.pmu.isCharging();
        out.vbus = instance.pmu.isVbusIn();
        return true;
    }

    void loop() override
    {
        if (vendorOk_) {
            I2cLock lock;   // instance.loop() reads PMU/sensor IRQ status over I2C
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
        auto has = [this](uint32_t mask) { return (probe_ & mask) != 0; };
        if (pending_ & (1u << uint8_t(p))) {
            return PeriphState::Pending;
        }
        if (absent_ & (1u << uint8_t(p))) {
            return PeriphState::Absent;
        }
        bool online;
        switch (p) {
        case Periph::Pmu: return pmuOk_ ? PeriphState::Ok : PeriphState::Failed;
        case Periph::Display: return displayOk_ ? PeriphState::Ok : PeriphState::Failed;
        case Periph::Touch: online = has(HW_TOUCH_ONLINE); break;
        case Periph::Rtc: online = has(HW_RTC_ONLINE); break;
        case Periph::Imu: online = has(HW_BHI260AP_ONLINE); break;
        case Periph::Haptics: online = has(HW_DRV_ONLINE); break;
        case Periph::Lora: online = has(HW_RADIO_ONLINE); break;
        case Periph::Nfc: online = has(HW_NFC_ONLINE); break;
        case Periph::Gnss: online = has(HW_GPS_ONLINE); break;
        case Periph::SdCard: online = has(HW_SD_ONLINE); break;
        case Periph::Expander: online = has(HW_EXPAND_ONLINE); break;
        case Periph::Mic:
        case Periph::Speaker:
            // No probe bit; both are gated by NO_HW_MIC in LilyGoLib.
            return SimFail::isSet(simMask_, Periph::Mic) ? PeriphState::SimulatedFail : PeriphState::Ok;
        default: return PeriphState::Absent;
        }
        if (online) {
            return PeriphState::Ok;
        }
        return SimFail::isSet(simMask_, p) ? PeriphState::SimulatedFail : PeriphState::Failed;
    }

    bool lvglReady() const override { return lvgl_; }

private:
    void stage(BootStage s)
    {
        if (stageCb_) {
            stageCb_(s);
        }
    }

    static uint32_t noHwFlags(uint16_t mask)
    {
        static const struct {
            Periph p;
            uint32_t flag;
        } map[] = {
            {Periph::Lora, NO_HW_LORA}, {Periph::Nfc, NO_HW_NFC},     {Periph::Gnss, NO_HW_GPS},
            {Periph::SdCard, NO_HW_SD}, {Periph::Mic, NO_HW_MIC},     {Periph::Haptics, NO_HW_DRV},
            {Periph::Imu, NO_HW_SENSOR}, {Periph::Rtc, NO_HW_RTC},
        };
        uint32_t f = 0;
        for (auto& m : map) {
            if (SimFail::isSet(mask, m.p)) {
                f |= m.flag;
            }
        }
        return f;
    }

    // SensorDataParseCallback; runs inside instance.sensor.update() on the UI task. Keep it short.
    static void onGravity(uint8_t, uint8_t* data, uint32_t size, uint64_t*, void* user)
    {
        auto* self = static_cast<BoardUltra*>(user);
        if (!self->gravityCb_ || size < 6) {
            return;
        }
        bhy2_data_xyz v;
        bhy2_parse_xyz(data, &v);
        GravitySample s{v.x * self->gravityScale_, v.y * self->gravityScale_, v.z * self->gravityScale_, millis()};
        self->gravityCb_(s, self->gravityCtx_);
    }

    static void onDeviceEvent(DeviceEvent_t event, void* params, void* user)
    {
        auto* self = static_cast<BoardUltra*>(user);
        if (event == POWER_EVENT && instance.getPMUEventType(params) == PMU_EVENT_KEY_CLICKED &&
            self->buttonCb_) {
            self->buttonCb_(self->buttonCtx_);
        }
    }

    // LVGL's touch read runs on the UI core; wrap it so it shares the I2C lock with core-0 services.
    void wrapTouchRead()
    {
        for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
            if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
                origTouchRead_ = lv_indev_get_read_cb(in);
                lv_indev_set_read_cb(in, &BoardUltra::lockedTouchRead);
                return;
            }
        }
    }
    static void lockedTouchRead(lv_indev_t* in, lv_indev_data_t* data)
    {
        I2cLock lock;
        if (origTouchRead_) {
            origTouchRead_(in, data);
        }
    }

    static lv_indev_read_cb_t origTouchRead_;
    volatile uint32_t probe_ = 0;
    volatile uint32_t pending_ = 0;
    volatile uint32_t absent_ = 0;    // present on the board but not inserted (e.g. no microSD card)   // bit per Periph still awaiting probeDeferred()
    uint16_t simMask_ = 0;
    bool pmuOk_ = true;
    bool vendorOk_ = false;
    bool displayOk_ = false;
    bool lvgl_ = false;
    StageCb stageCb_ = nullptr;
    GravityCb gravityCb_ = nullptr;
    void* gravityCtx_ = nullptr;
    float gravityScale_ = 1.0f / 4096.0f;
    ButtonCb buttonCb_ = nullptr;
    void* buttonCtx_ = nullptr;
};

lv_indev_read_cb_t BoardUltra::origTouchRead_ = nullptr;
BoardUltra g_board;

} // namespace

IBoard& board() { return g_board; }

#endif // ARDUINO_T_WATCH_S3_ULTRA
