// AeroDeck board HAL. The only layer allowed to touch LilyGoLib/driver types (Principle II).
// Contract: specs/001-platform-foundation/contracts/internal_interfaces.md
#pragma once
#include <ctime>
#include "core/BootGuard.h"
#include "core/Types.h"

struct GravitySample {
    float x, y, z;   // gravity in the sensor frame (after LilyGoLib's axis remap); only ratios matter in S1
    uint32_t ms;     // millis() when the sample was received
};

class IBoard {
public:
    using GravityCb = void (*)(const GravitySample& s, void* ctx);
    using ButtonCb = void (*)(void* ctx);
    using StageCb = void (*)(core::BootStage stage);

    // Called by begin() just before each boot stage (PmuProbe, VendorInit, DisplayInit) for BootGuard.
    virtual void onStage(StageCb cb) = 0;

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
    virtual ~IBoard() = default;
};

IBoard& board();   // defined once per board in src/board/Board<Name>.cpp
