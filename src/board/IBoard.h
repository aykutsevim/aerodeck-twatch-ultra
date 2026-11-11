// AeroDeck board HAL. The only layer allowed to touch LilyGoLib/driver types (Principle II).
// Contract: specs/001-platform-foundation/contracts/internal_interfaces.md
#pragma once
#include "core/BootGuard.h"
#include "core/Types.h"

class IBoard {
public:
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
    virtual ~IBoard() = default;
};

IBoard& board();   // defined once per board in src/board/Board<Name>.cpp
