#include "core/Types.h"

namespace core {

const char* wakeCauseName(WakeCause c)
{
    switch (c) {
    case WakeCause::PowerOn: return "poweron";
    case WakeCause::Button: return "button";
    case WakeCause::Imu: return "imu";
    case WakeCause::Timer: return "timer";
    default: return "other";
    }
}

const char* periphName(Periph p)
{
    switch (p) {
    case Periph::Pmu: return "PMU";
    case Periph::Display: return "DISPLAY";
    case Periph::Touch: return "TOUCH";
    case Periph::Rtc: return "RTC";
    case Periph::Imu: return "IMU";
    case Periph::Haptics: return "HAPTICS";
    case Periph::Lora: return "LORA";
    case Periph::Nfc: return "NFC";
    case Periph::Gnss: return "GNSS";
    case Periph::SdCard: return "SD";
    case Periph::Mic: return "MIC";
    case Periph::Speaker: return "SPEAKER";
    case Periph::Expander: return "EXPANDER";
    default: return "?";
    }
}

const char* periphStateName(PeriphState s)
{
    switch (s) {
    case PeriphState::Ok: return "OK";
    case PeriphState::Failed: return "FAILED";
    case PeriphState::Absent: return "ABSENT";
    case PeriphState::SimulatedFail: return "SIMFAIL";
    case PeriphState::Pending: return "PENDING";
    default: return "?";
    }
}

bool periphOptional(Periph p)
{
    return !(p == Periph::Pmu || p == Periph::Display || p == Periph::Touch);
}

} // namespace core
