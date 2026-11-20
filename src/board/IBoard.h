// AeroDeck board HAL. The only layer allowed to touch LilyGoLib/driver types (Principle II).
// Contract: specs/001-platform-foundation/contracts/internal_interfaces.md
#pragma once
#include <cstddef>
#include <cstdint>
#include <ctime>
#include "core/AlphaArt.h"
#include "core/BootGuard.h"
#include "core/SplashCompose.h"
#include "core/Types.h"

struct GravitySample {
    float x, y, z;   // gravity in the sensor frame (after LilyGoLib's axis remap); only ratios matter in S1
    uint32_t ms;     // millis() when the sample was received
};

// S6: what configureMotion() could enable (the service logs it).
struct MotionInfo {
    bool steps;          // hub step counter running
    bool accel;          // accelerometer (incl. gravity) running
    float accelScale;    // LSB → g as applied
    float rangeG;        // measuring range in g (0 = unknown)
};

// S9: LoRa radio profile and received packet (contracts/board_radio.md).
struct RadioProfile {
    float freqMHz;
    float bwKHz;
    uint8_t sf;
    uint8_t crDenom;
    int8_t powerDbm;
    uint8_t syncWord;
    uint16_t preamble;
};
struct RadioPacket {
    uint8_t data[64];
    uint8_t len;
    int16_t rssi;
    int8_t snrQ4;     // SNR × 4 (dB)
    bool crcError;
};

// Feature 011: boot splash pushed straight to the panel right after its init (contracts/board_splash.md).
struct SplashSpec {
    const AlphaArt* art = nullptr;   // nullptr = no splash
    uint16_t fg565 = 0xFFFF;         // day white; NVG: core::NvgColor::toGreen565(0xFFFF)
    uint8_t brightnessPct = 80;      // the value applyBrightness() will set later (no flicker)
};
struct SplashResult {
    bool shown;
    uint32_t atMs;     // millis() when it became visible
    uint32_t costMs;   // compose + push time
    core::SplashSkip skip;
};

class IBoard {
public:
    using GravityCb = void (*)(const GravitySample& s, void* ctx);
    using ButtonCb = void (*)(void* ctx);
    using StageCb = void (*)(core::BootStage stage);

    // Called by begin() just before each boot stage (PmuProbe, VendorInit, DisplayInit) for BootGuard.
    virtual void onStage(StageCb cb) = 0;

    // Feature 011: set before begin(). Boards without a splash keep the defaults (never shown).
    virtual void setSplash(const SplashSpec&) {}
    virtual SplashResult splashResult() const { return {false, 0, 0, core::SplashSkip::NoArt}; }

    // Never aborts on optional hardware. headless skips display/LVGL init entirely.
    virtual core::BootResult begin(uint16_t simFailMask, bool safeMode, bool headless) = 0;
    virtual uint32_t caps() const = 0;
    virtual const char* name() const = 0;
    virtual core::DisplayInfo display() const = 0;
    virtual void setBrightnessPct(uint8_t pct) = 0;
    virtual void setRail(core::Rail r, bool on) = 0;     // PowerService only
    virtual bool readPmu(core::PmuReading& out) = 0;    // takes the I2C lock
    virtual void loop() = 0;                            // UI task; pumps vendor events
    virtual void onButtonClick(ButtonCb cb, void* ctx) = 0;
    virtual core::PeriphState periph(core::Periph p) const = 0;
    virtual bool lvglReady() const = 0;
    // Probes slow peripherals skipped by begin() to meet the boot target (FR-001, ≤4 s).
    // Blocking (BHI260AP firmware upload ~2 s); call from a background task on core 0.
    virtual void probeDeferred() = 0;
    // S1: BHI260AP gravity virtual sensor. The callback runs on the UI task inside loop() (I2C lock held).
    // Returns false if the IMU is not Ok or the board has no fusion sensor.
    virtual bool configureAttitude(float rateHz, GravityCb cb, void* ctx) = 0;
    virtual bool readRtc(std::tm& utc, bool& integrity) = 0;   // RTC holds UTC
    virtual bool writeRtc(const std::tm& utc) = 0;
    // S4: DRV2605 sequence (<= 7 entries: ROM effect ids, 0x80|n = wait n x 10 ms). False if haptics unavailable.
    virtual bool haptic(const uint8_t* seq, uint8_t n) = 0;
    virtual void hapticStop() = 0;
    // S5: I2S speaker output through LilyGoLib's Player (legacy driver installed at boot). Audio task only.
    // audioBegin: set the clock for 16-bit mono at rateHz and clear the DMA buffers; false if no speaker.
    // audioWrite: blocks until the DMA accepted all n samples; returns the samples written.
    // audioEnd: zero the DMA buffers so no stale samples replay.
    virtual bool audioBegin(uint32_t rateHz) = 0;
    virtual size_t audioWrite(const int16_t* s, size_t n) = 0;
    virtual void audioEnd() = 0;
    // S6: flight recorder. configureMotion after the IMU is up (deferred probe): step counter 1 Hz + accelerometer
    // at accelHz. Callbacks run on the UI task (inside loop()) and only store values. stepCount: the hub's count
    // since it started. takePeakG: the peak |a| in g since the previous call (then reset), the latest |a| and the
    // range. Both return false when the source is unavailable.
    virtual bool configureMotion(float accelHz, MotionInfo& info) = 0;
    virtual bool stepCount(uint32_t& out) = 0;
    virtual bool takePeakG(float& peak, float& last, float& rangeG) = 0;
    // S7 (contracts/board_interface.md). UI task: false = AMOLED sleep-in + panel enable low (rail kept on),
    // true = wake. Takes the I2C lock.
    virtual void setDisplayOn(bool on) = 0;
    // S7: while on, every flushed LVGL buffer is recoloured by core::NvgColor::apply before it is pushed.
    virtual void setNvgFilter(bool on) = 0;
    // S7: svc task, last step of PowerService::eject(). wakeMask: core::kWakeButton | kWakeImu | kWakeTimer;
    // timerSec > 0 adds a timer wake. Never returns on success; false = deep sleep unsupported on this board.
    virtual bool deepSleep(uint8_t wakeMask, uint32_t timerSec) = 0;
    // S7: why this boot happened (cached at begin()).
    virtual core::WakeCause wakeCause() const = 0;
    // Diagnostics (`ad probe s7 i2c`): addresses that ACK on the shared I2C bus. Default: not supported.
    virtual uint8_t i2cScan(uint8_t* out, uint8_t max) { (void)out; (void)max; return 0; }

    // S9 LoRa radio (specs/010-tcas-lora-radar/contracts/board_radio.md). Called from the `radio` task (core 0),
    // except radioAirtimeMs/nodeId. The rail must already be on (RailLease(Lora) held by the caller).
    // radioStart: configure the SX1262, built-in antenna, DIO1 ISR, continuous RX. false = no radio / failed.
    virtual bool radioStart(const RadioProfile& p) = 0;
    virtual void radioStop() = 0;                                          // sleep the chip, detach the ISR
    virtual bool radioSend(const uint8_t* buf, size_t n, uint32_t& airMs) = 0;   // blocking, then back to RX
    virtual bool radioPoll(RadioPacket& out) = 0;                          // true = packet or CRC error since last
    virtual bool radioWait(uint32_t timeoutMs) = 0;                        // DIO1 notification or timeout
    virtual uint32_t radioAirtimeMs(size_t n) = 0;                         // for the current profile
    virtual int radioLastError() const { return 0; }                       // RadioLib status of the last call
    virtual uint32_t nodeId() const = 0;                                   // low 32 bits of the eFuse MAC
    virtual ~IBoard() = default;
};

IBoard& board();   // defined once per board in src/board/Board<Name>.cpp
