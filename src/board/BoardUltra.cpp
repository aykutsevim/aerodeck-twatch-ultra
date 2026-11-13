// T-Watch Ultra implementation of IBoard (wraps LilyGoLib `instance`).
#ifdef ARDUINO_T_WATCH_S3_ULTRA

#include <Arduino.h>
#include <Wire.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include "board/IBoard.h"
#include "board/I2cLock.h"
#include "board/I2cRecover.h"
#include "core/NvgColor.h"
#include "core/SimFail.h"
#include <bosch/bhy2_parse.h>
#include <esp_sleep.h>

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
        wakeCause_ = readWakeCause();

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
        audioReady_ = !(flags & NO_HW_MIC);   // initAmplifier() ran (Player I2S installed)
        vendorOk_ = true;

        stage(BootStage::DisplayInit);
        beginLvglHelper(instance);
        wrapTouchRead();
        wrapFlush();
        lvgl_ = true;
        displayOk_ = true;

        if (wakeCause_ == WakeCause::Button) {
            I2cLock lock;
            instance.pmu.clearIrqStatus();   // S7: the wake press must not arrive again as a click (R6)
        }
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

    // S6: flight recorder sources on the BHI260AP (research R1/R2).
    bool configureMotion(float accelHz, MotionInfo& info) override
    {
        info = MotionInfo{false, false, 0.0f, 0.0f};
        if (periph(Periph::Imu) != PeriphState::Ok) {
            return false;
        }
        I2cLock lock;
        info.steps = instance.sensor.onResultEvent(SensorBHI260AP::STEP_COUNTER, &BoardUltra::onStep, this) &&
                     instance.sensor.configure(BHY2_SENSOR_ID_STC, 1.0f, 0);
        instance.sensor.configureRange(BHY2_SENSOR_ID_ACC, 16);   // Â±16 g if the firmware accepts it
        float scale = instance.sensor.getScaling(BHY2_SENSOR_ID_ACC);
        if (!(scale > 0.0f)) {
            scale = 1.0f / 4096.0f;
        }
        info.accel = instance.sensor.onResultEvent(SensorBHI260AP::ACCEL_CORRECTED, &BoardUltra::onAccel, this) &&
                     instance.sensor.configure(BHY2_SENSOR_ID_ACC, accelHz, 0);
        const SensorConfig cfg = instance.sensor.getConfigure(BHY2_SENSOR_ID_ACC);
        if (cfg.range > 0) {
            // getScaling() keeps reporting the default Â±8 g step (1/4096 g) after configureRange: measured at rest
            // |a| = 0.50 with Â±16 g (S6 T012). Full scale = range over the int16 span.
            scale = float(cfg.range) / 32768.0f;
        }
        accelScale_ = scale;
#ifdef AERODECK_IMU_WAKE
        // S7 spike (research R7): which wake-up sensors does the loaded firmware offer?
        {
            const uint8_t ids[] = {BHY2_SENSOR_ID_WRIST_TILT_GESTURE, BHY2_SENSOR_ID_WAKE_GESTURE, BHY2_SENSOR_ID_SIG};
            for (uint8_t id : ids) {
                const bool present = instance.sensor.onResultEvent(SensorBHI260AP::BoschSensorID(id), &BoardUltra::onWakeEvent, this);
                log_i("imuwake: sensor %u %s", unsigned(id), present ? "present" : "absent");
                if (present && !wakeSensor_) {
                    wakeSensor_ = id;
                }
            }
        }
#endif
        info.accelScale = scale;
        info.rangeG = float(cfg.range);
        rangeG_ = info.rangeG;
        stepsOk_ = info.steps;
        accelOk_ = info.accel;
        return info.steps || info.accel;
    }

    bool stepCount(uint32_t& out) override
    {
        if (!stepsOk_) {
            return false;
        }
        out = stepSeen_ ? steps_ : 0;   // no event yet: the hub has counted nothing since it started
        return true;
    }

    bool takePeakG(float& peak, float& last, float& rangeG) override
    {
        if (!accelOk_) {
            return false;
        }
        portENTER_CRITICAL(&motionLock_);
        peak = peakG_;
        last = lastG_;
        peakG_ = 0.0f;
        portEXIT_CRITICAL(&motionLock_);
        rangeG = rangeG_;
        return true;
    }

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
                    // Card-detect via the expander: HIGH = no card inserted â†’ ABSENT, not FAILED.
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
        uint32_t c = Cap::Lora | Cap::Haptics | Cap::Speaker | Cap::Mic | Cap::Nfc | Cap::Gnss | Cap::SdCard |
                     Cap::ImuStepCounter | Cap::Amoled;
#ifdef AERODECK_IMU_WAKE
        if (wakeSensor_) {
            c |= uint32_t(Cap::ImuWakeInterrupt);   // research R7 spike (default off until promoted, ADR-014)
        }
#endif
        return c;
    }

    const char* name() const override { return "T-Watch Ultra"; }

    DisplayInfo display() const override
    {
        // Rotated size as LVGL sees it. DISP_WIDTH/DISP_HEIGHT (502Ã—410) describe the panel before rotation;
        // LilyGoLib's default rotation 0 is portrait, i.e. 410 wide Ã— 502 tall.
        // The Ultra AMOLED has rounded corners with a 64 px radius.
        return DisplayInfo{instance.width(), instance.height(), false, 64};
    }

    void setBrightnessPct(uint8_t pct) override
    {
        if (pct > 100) {
            pct = 100;
        }
        waitFlushIdle();   // S7: brightness is a QSPI panel command; the flush task may be using the bus
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

    // S7: screen off/on while running (research R4). The display rail stays on (LilyGoLib advice).
    void setDisplayOn(bool on) override
    {
        if (!displayOk_ || on == displayOn_) {
            return;
        }
        if (!on) {
            waitFlushIdle();   // never sleep the panel in the middle of a transfer
        }
        I2cLock lock;   // the panel enable is on the XL9555 expander
        if (on) {
            instance.wakeupDisplay();
        } else {
            instance.sleepDisplay();
        }
        displayOn_ = on;
    }

    void setNvgFilter(bool on) override { g_nvgOn = on; }

    // S7: deep sleep without LilyGoLib's sleep() (2 s delay; timer XOR pins; sensor rail off). Research R6.
    bool deepSleep(uint8_t wakeMask, uint32_t timerSec) override
    {
        if (!vendorOk_) {
            return false;
        }
        waitFlushIdle();
        I2cLock lock;
        instance.pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        if (wakeMask & kWakeButton) {
            instance.pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
        }
        instance.touch.sleep();
        instance.LilyGoDispQSPI::sleep();
        instance.LilyGoDispQSPI::end();
        instance.uninstallSD();
        const uint8_t expands[] = {EXPANDS_DRV_EN, EXPANDS_DISP_EN};
        for (uint8_t pin : expands) {
            instance.io.digitalWrite(pin, LOW);
        }
        instance.disablePowerMeasure();
        instance.pmu.enableSleep();
        // The display rail (ALDO2) stays on: cutting it raises the sleep current by ~600 ÂµA (LilyGoLib note).
        instance.powerControl(POWER_HAPTIC_DRIVER, false);
        instance.powerControl(POWER_GPS, false);
        instance.powerControl(POWER_SPEAK, false);
        instance.powerControl(POWER_NFC, false);
        instance.powerControl(POWER_SD_CARD, false);
        instance.powerControl(POWER_RADIO, false);
        // Sensor hub: cutting its rail (as LilyGoLib's sleep() does) leaves the BHI260AP delivering frozen
        // gravity/accelerometer values after the wake boot (validation T006). Keep it powered and idle instead:
        // stop its virtual sensors so it draws only its sleep current.
        if (periph(Periph::Imu) == PeriphState::Ok) {
            instance.sensor.configure(BHY2_SENSOR_ID_GRA, 0, 0);
            instance.sensor.configure(BHY2_SENSOR_ID_ACC, 0, 0);
            instance.sensor.configure(BHY2_SENSOR_ID_STC, 0, 0);
#ifdef AERODECK_IMU_WAKE
            // Spike (research R7): only the wake-up FIFO may assert the (active-low) INT line on GPIO8.
            if ((wakeMask & kWakeImu) && wakeSensor_) {
                instance.sensor.setInterruptCtrl(BHY2_ICTL_DISABLE_FIFO_NW | BHY2_ICTL_DISABLE_STATUS_FIFO |
                                                 BHY2_ICTL_DISABLE_DEBUG | BHY2_ICTL_ACTIVE_LOW);
                instance.sensor.configure(wakeSensor_, 1.0f, 0);
            }
#endif
        }
        // RTC backup charging stays on (unlike the vendor default): the clock must survive.
        instance.pmu.clearIrqStatus();

        Serial1.end();
        SPI.end();
        Wire.end();
        const uint8_t pins[] = {DISP_D0, DISP_D1, DISP_D2, DISP_D3, DISP_SCK, DISP_CS, DISP_TE, 37,
                                RTC_INT, NFC_INT, NFC_CS, MIC_SCK, MIC_DAT, I2S_BCLK, I2S_WCLK, I2S_DOUT,
                                SD_CS, SDA, SCL, MOSI, MISO, SCK, GPS_TX, GPS_RX, GPS_PPS,
                                LORA_CS, LORA_RST, LORA_BUSY, LORA_IRQ, TP_INT};
        for (uint8_t pin : pins) {
            gpio_reset_pin(gpio_num_t(pin));
            pinMode(pin, OPEN_DRAIN);
        }
        uint64_t ext1 = 0;
        if (wakeMask & kWakeButton) {
            ext1 |= 1ULL << PMU_INT;
        }
        if (wakeMask & kWakeImu) {
            ext1 |= 1ULL << SENSOR_INT;
        } else {
            gpio_reset_pin(gpio_num_t(SENSOR_INT));
            pinMode(SENSOR_INT, OPEN_DRAIN);
        }
        if (ext1) {
            esp_sleep_enable_ext1_wakeup(ext1, ESP_EXT1_WAKEUP_ANY_LOW);
        }
        if ((wakeMask & kWakeTimer) && timerSec > 0) {
            esp_sleep_enable_timer_wakeup(uint64_t(timerSec) * 1000000ULL);
        }
        esp_deep_sleep_start();
        return false;   // not reached
    }

    WakeCause wakeCause() const override { return wakeCause_; }

    uint8_t i2cScan(uint8_t* out, uint8_t max) override
    {
        uint8_t n = 0;
        I2cLock lock;
        for (uint8_t a = 1; a < 127 && n < max; a++) {
            Wire.beginTransmission(a);
            if (Wire.endTransmission() == 0) {
                out[n++] = a;
            }
        }
        return n;
    }

private:
    static WakeCause readWakeCause()
    {
        switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t st = esp_sleep_get_ext1_wakeup_status();
            if (st & (1ULL << PMU_INT)) {
                return WakeCause::Button;
            }
            if (st & (1ULL << SENSOR_INT)) {
                return WakeCause::Imu;
            }
            return WakeCause::Other;
        }
        case ESP_SLEEP_WAKEUP_TIMER: return WakeCause::Timer;
        case ESP_SLEEP_WAKEUP_UNDEFINED: return WakeCause::PowerOn;
        default: return WakeCause::Other;
        }
    }

    // S7: our own flush, replacing LilyGoLib's disp_flush (LV_Helper_v9.cpp): same _SWAP_COLORS byte swap and
    // pushColors, plus the NVG filter (research R1). Reading the original callback would need LVGL's private
    // display struct, which is avoided on purpose.
    // Asynchronous: the flush callback (UI task) hands the buffer to a core-0 task that recolours/swaps and pushes
    // it (pushColors blocks on the QSPI DMA). LVGL meanwhile renders the next area into its second buffer. Before
    // it reuses a buffer, LVGL calls flushWait(), which blocks until the task is done with the previous flush.
    struct FlushJob {
        lv_display_t* d;
        lv_area_t area;
        uint8_t* px;
    };
    void wrapFlush()
    {
        lv_display_t* d = lv_display_get_default();
        if (!d) {
            return;
        }
        s_flushQ = xQueueCreate(1, sizeof(FlushJob));
        s_flushDone = xSemaphoreCreateBinary();
        if (s_flushQ && s_flushDone &&
            xTaskCreatePinnedToCore(flushTask, "flush", 3072, nullptr, 10, nullptr, 0) == pdPASS) {
            lv_display_set_flush_cb(d, &BoardUltra::asyncFlush);
            lv_display_set_flush_wait_cb(d, &BoardUltra::flushWait);
        } else {
            lv_display_set_flush_cb(d, &BoardUltra::syncFlush);   // fallback: same work on the UI task
        }
    }
    static void convertAndPush(lv_display_t* d, const lv_area_t& a, uint8_t* px)
    {
        const uint32_t n = lv_area_get_size(&a);
        // The panel wants byte-swapped RGB565. In NVG the recolour writes swapped pixels directly (one pass).
        if (g_nvgOn) {
            core::NvgColor::applySwapped(reinterpret_cast<uint16_t*>(px), n);
        } else {
            lv_draw_sw_rgb565_swap(px, n);
        }
        auto* plane = static_cast<LilyGo_Display*>(lv_display_get_user_data(d));
        plane->pushColors(a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a), reinterpret_cast<uint16_t*>(px));
    }
    static void syncFlush(lv_display_t* d, const lv_area_t* area, uint8_t* px)
    {
        convertAndPush(d, *area, px);
        lv_display_flush_ready(d);
    }
    static void asyncFlush(lv_display_t* d, const lv_area_t* area, uint8_t* px)
    {
        FlushJob j{d, *area, px};
        g_flushBusy = true;
        xQueueSend(s_flushQ, &j, portMAX_DELAY);
    }
    // LVGL calls this only while disp->flushing is set (one call per flush) and clears the flag afterwards.
    static void flushWait(lv_display_t*) { xSemaphoreTake(s_flushDone, portMAX_DELAY); }
    static void flushTask(void*)
    {
        FlushJob j;
        for (;;) {
            if (xQueueReceive(s_flushQ, &j, portMAX_DELAY) == pdTRUE) {
                convertAndPush(j.d, j.area, j.px);
                g_flushBusy = false;
                xSemaphoreGive(s_flushDone);
            }
        }
    }
    // UI task: before the panel is put to sleep, let an in-flight transfer finish.
    static void waitFlushIdle()
    {
        for (int i = 0; i < 100 && g_flushBusy; i++) {
            vTaskDelay(1);
        }
    }
    static volatile bool g_nvgOn;
    static volatile bool g_flushBusy;
    static QueueHandle_t s_flushQ;
    static SemaphoreHandle_t s_flushDone;

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

    static void onWakeEvent(uint8_t id, uint8_t*, uint32_t, uint64_t*, void*) { log_i("imuwake: event %u", id); }

    static void onStep(uint8_t, uint8_t* data, uint32_t size, uint64_t*, void* user)
    {
        auto* self = static_cast<BoardUltra*>(user);
        if (size < 4) {
            return;
        }
        self->steps_ = bhy2_parse_step_counter(data);
        self->stepSeen_ = true;
    }

    static void onAccel(uint8_t, uint8_t* data, uint32_t size, uint64_t*, void* user)
    {
        auto* self = static_cast<BoardUltra*>(user);
        if (size < 6) {
            return;
        }
        bhy2_data_xyz v;
        bhy2_parse_xyz(data, &v);
        const float x = v.x * self->accelScale_, y = v.y * self->accelScale_, z = v.z * self->accelScale_;
        const float g = sqrtf(x * x + y * y + z * z);
        portENTER_CRITICAL(&self->motionLock_);
        self->lastG_ = g;
        if (g > self->peakG_) {
            self->peakG_ = g;
        }
        portEXIT_CRITICAL(&self->motionLock_);
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
    bool audioReady_ = false;   // S5: Player I2S driver installed
    bool displayOn_ = true;     // S7
    volatile uint8_t wakeSensor_ = 0;   // S7 spike: hub wake-up sensor id (0 = none)
    WakeCause wakeCause_ = WakeCause::PowerOn;
    // S6 motion (written on the UI task by the hub callbacks, read by the recorder on the svc task)
    portMUX_TYPE motionLock_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t steps_ = 0;
    volatile bool stepSeen_ = false;
    volatile bool stepsOk_ = false;
    volatile bool accelOk_ = false;
    float peakG_ = 0.0f;
    float lastG_ = 0.0f;
    float accelScale_ = 1.0f / 4096.0f;
    float rangeG_ = 0.0f;
    StageCb stageCb_ = nullptr;
    GravityCb gravityCb_ = nullptr;
    void* gravityCtx_ = nullptr;
    float gravityScale_ = 1.0f / 4096.0f;
    ButtonCb buttonCb_ = nullptr;
    void* buttonCtx_ = nullptr;
};

lv_indev_read_cb_t BoardUltra::origTouchRead_ = nullptr;
volatile bool BoardUltra::g_nvgOn = false;
volatile bool BoardUltra::g_flushBusy = false;
QueueHandle_t BoardUltra::s_flushQ = nullptr;
SemaphoreHandle_t BoardUltra::s_flushDone = nullptr;
BoardUltra g_board;

} // namespace

IBoard& board() { return g_board; }

#endif // ARDUINO_T_WATCH_S3_ULTRA
