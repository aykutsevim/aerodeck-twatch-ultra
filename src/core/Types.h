// AeroDeck core: plain shared types. Pure C++, no Arduino/LVGL/vendor includes (Principle II).
#pragma once
#include <cstdint>

namespace core {

enum class Cap : uint32_t {
    Lora = 1u << 0, Haptics = 1u << 1, Speaker = 1u << 2, Mic = 1u << 3, IrTx = 1u << 4,
    Nfc = 1u << 5, Gnss = 1u << 6, SdCard = 1u << 7, ImuStepCounter = 1u << 8,
    ImuWakeInterrupt = 1u << 9, Amoled = 1u << 10
};
constexpr uint32_t operator|(Cap a, Cap b) { return uint32_t(a) | uint32_t(b); }
constexpr uint32_t operator|(uint32_t a, Cap b) { return a | uint32_t(b); }
constexpr bool hasCap(uint32_t caps, Cap c) { return (caps & uint32_t(c)) != 0; }

enum class Rail : uint8_t { Display, Lora, AudioAmp, Gnss, Nfc, Sensor, Count };

enum class Periph : uint8_t {
    Pmu, Display, Touch, Rtc, Imu, Haptics, Lora, Nfc, Gnss, SdCard, Mic, Speaker, Expander, Count
};
enum class PeriphState : uint8_t { Ok, Failed, Absent, SimulatedFail, Pending };

struct DisplayInfo { uint16_t w; uint16_t h; bool round; };

// battmA is INT16_MIN when the PMU cannot measure battery current (AXP2101 has no such ADC).
struct PmuReading {
    uint8_t battPct;
    uint16_t battmV;
    int16_t battmA;
    int16_t dieTempC10;
    bool charging;
    bool vbus;
};
constexpr int16_t kCurrentUnavailable = INT16_MIN;

struct BootResult { bool pmuOk; bool safeMode; uint16_t failedMask; };

const char* periphName(Periph p);
const char* periphStateName(PeriphState s);
bool periphOptional(Periph p);

} // namespace core
