// AeroDeck core: drop-tank chronograph state machine (S8, FR-001…011, research R1, data-model §2). Pure.
// Countdown and Pomodoro on absolute UTC milliseconds: a running timer stores its end time, a paused one its
// remaining time, so page changes, screen-off, restarts and deep sleep never lose time.
#pragma once
#include <cstdint>

namespace core {

class ChronoModel {
public:
    enum class Mode : uint8_t { Countdown, Pomodoro };
    enum class State : uint8_t { Idle, Running, Paused, Expired };
    enum class Phase : uint8_t { Countdown, Work, Break };
    enum class Event : uint8_t { None, Expired, WorkDone, BreakDone };

    struct Config {
        Mode mode;
        int64_t countdownMs;   // setting chrlen
        int64_t workMs;        // setting chrwork
        int64_t breakMs;       // setting chrbrk
    };

    // Everything needed to restore after a restart (ChronoBlob).
    struct Raw {
        Mode mode;
        State state;
        Phase phase;
        uint16_t cycles;
        bool late;
        int64_t intervalMs;
        int64_t endUtcMs;
        int64_t remainingMs;
        int64_t countdownMs;
        int64_t workMs;
        int64_t breakMs;
    };

    ChronoModel();
    void configure(const Config& c);   // applied only while Idle or Expired

    void start(int64_t now);    // Paused: same as resume
    void pause(int64_t now);
    void resume(int64_t now);
    void reset();
    Event tick(int64_t now);     // one transition per call
    Event restore(int64_t now);  // after a load: catch up on everything missed (late), clamp clock resets

    State state() const { return r_.state; }
    Mode mode() const { return r_.mode; }
    Phase phase() const { return r_.phase; }
    uint16_t cycles() const { return r_.cycles; }
    bool late() const { return r_.late; }
    int64_t intervalMs() const { return r_.intervalMs; }
    int64_t remainingMs(int64_t now) const;
    float fraction(int64_t now) const;
    int64_t nextEventUtcMs() const;   // -1 when not running

    Raw raw() const { return r_; }
    void load(const Raw& r) { r_ = r; }

private:
    Phase firstPhase() const { return r_.mode == Mode::Pomodoro ? Phase::Work : Phase::Countdown; }
    int64_t lengthOf(Phase p) const;
    Event advance();   // Running and the current phase ended: next phase or Expired

    Raw r_;
};

} // namespace core
