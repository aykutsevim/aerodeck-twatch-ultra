// AeroDeck core: display power policy (S7, FR-001…005, data-model §2). Pure; host-tested.
// Events (button, raise, alert, console) request a wake; update() turns them into actions and handles the
// idle timeout and the touch suppression right after a wake.
#pragma once
#include <cstdint>
#include "core/Alerts.h"

namespace core {

class ScreenPolicy {
public:
    enum class Action : uint8_t { None, TurnOff, TurnOn };
    enum class Cause : uint8_t { Boot, Button, Raise, Alert, Touch, Cmd };

    static constexpr uint8_t kTimeoutCount = 6;   // 10 s, 15 s, 30 s, 1 min, 5 min, Never
    static constexpr uint32_t kSuppressMs = 300;
    static uint32_t timeoutMs(uint8_t idx);       // 0 = Never; out of range → 15 s
    static const char* causeName(Cause c);

    void configure(uint8_t timeoutIdx, bool raiseEnabled, bool raiseAvailable);
    void begin(uint32_t nowMs);

    bool onButton(uint32_t nowMs);                // true = consumed (it only woke the screen)
    bool onRaise(uint32_t nowMs);                 // true = will turn on
    bool onAlert(AlertLevel lvl, uint32_t nowMs); // Caution/Warning; true = will turn on
    void command(bool on, uint32_t nowMs);        // console test hook

    // inactiveMs: time since the last touch input; touchDown: a finger is on the glass now.
    Action update(uint32_t nowMs, uint32_t inactiveMs, bool setupShown, bool touchDown);

    bool on() const { return on_; }
    Cause cause() const { return cause_; }
    bool suppressTouch() const { return suppress_; }
    uint8_t timeoutIdx() const { return timeoutIdx_; }
    bool raiseEnabled() const { return raiseEnabled_; }
    bool raiseAvailable() const { return raiseAvailable_; }
    uint32_t idleMs(uint32_t nowMs, uint32_t inactiveMs) const;

private:
    void requestOn(Cause c, uint32_t nowMs);

    uint8_t timeoutIdx_ = 1;
    bool raiseEnabled_ = true;
    bool raiseAvailable_ = false;
    bool on_ = true;
    Cause cause_ = Cause::Boot;
    uint32_t lastWakeMs_ = 0;
    Action pending_ = Action::None;
    bool suppress_ = false;
    bool sawDown_ = false;
    uint32_t suppressSinceMs_ = 0;
};

} // namespace core
