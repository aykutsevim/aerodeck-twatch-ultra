#include "core/ChronoModel.h"

namespace core {

ChronoModel::ChronoModel()
{
    r_ = Raw{Mode::Countdown, State::Idle, Phase::Countdown, 0, false, 25 * 60000LL, 0, 25 * 60000LL,
             25 * 60000LL, 25 * 60000LL, 5 * 60000LL};
}

int64_t ChronoModel::lengthOf(Phase p) const
{
    switch (p) {
    case Phase::Work: return r_.workMs;
    case Phase::Break: return r_.breakMs;
    default: return r_.countdownMs;
    }
}

void ChronoModel::configure(const Config& c)
{
    if (r_.state == State::Running || r_.state == State::Paused) {
        return;
    }
    r_.mode = c.mode;
    r_.countdownMs = c.countdownMs > 0 ? c.countdownMs : 60000;
    r_.workMs = c.workMs > 0 ? c.workMs : 60000;
    r_.breakMs = c.breakMs > 0 ? c.breakMs : 60000;
    if (r_.state == State::Idle) {
        r_.phase = firstPhase();
        r_.intervalMs = lengthOf(r_.phase);
        r_.remainingMs = r_.intervalMs;
    }
}

void ChronoModel::start(int64_t now)
{
    if (r_.state == State::Paused) {
        resume(now);
        return;
    }
    if (r_.state == State::Running) {
        return;
    }
    r_.phase = firstPhase();
    r_.intervalMs = lengthOf(r_.phase);
    r_.cycles = 0;
    r_.late = false;
    r_.endUtcMs = now + r_.intervalMs;
    r_.remainingMs = r_.intervalMs;
    r_.state = State::Running;
}

void ChronoModel::pause(int64_t now)
{
    if (r_.state != State::Running) {
        return;
    }
    r_.remainingMs = remainingMs(now);
    r_.state = State::Paused;
}

void ChronoModel::resume(int64_t now)
{
    if (r_.state != State::Paused) {
        return;
    }
    r_.endUtcMs = now + r_.remainingMs;
    r_.state = State::Running;
}

void ChronoModel::reset()
{
    r_.state = State::Idle;
    r_.phase = firstPhase();
    r_.intervalMs = lengthOf(r_.phase);
    r_.remainingMs = r_.intervalMs;
    r_.cycles = 0;
    r_.late = false;
}

ChronoModel::Event ChronoModel::advance()
{
    switch (r_.phase) {
    case Phase::Work:
        r_.cycles++;
        r_.phase = Phase::Break;
        r_.intervalMs = r_.breakMs;
        r_.endUtcMs += r_.breakMs;   // schedule from the previous end: no drift
        return Event::WorkDone;
    case Phase::Break:
        r_.phase = Phase::Work;
        r_.intervalMs = r_.workMs;
        r_.endUtcMs += r_.workMs;
        return Event::BreakDone;
    default:
        r_.state = State::Expired;
        r_.remainingMs = 0;
        return Event::Expired;
    }
}

ChronoModel::Event ChronoModel::tick(int64_t now)
{
    if (r_.state != State::Running || now < r_.endUtcMs) {
        return Event::None;
    }
    return advance();
}

ChronoModel::Event ChronoModel::restore(int64_t now)
{
    if (r_.state != State::Running) {
        return Event::None;
    }
    if (r_.endUtcMs - now > r_.intervalMs) {   // the clock went backwards (RTC lost): never more than a full phase
        r_.endUtcMs = now + r_.intervalMs;
        return Event::None;
    }
    Event last = Event::None;
    while (r_.state == State::Running && now >= r_.endUtcMs) {
        last = advance();
        r_.late = true;
    }
    return last;
}

int64_t ChronoModel::remainingMs(int64_t now) const
{
    switch (r_.state) {
    case State::Running: {
        int64_t r = r_.endUtcMs - now;
        if (r < 0) {
            r = 0;
        }
        if (r > r_.intervalMs) {
            r = r_.intervalMs;
        }
        return r;
    }
    case State::Paused: return r_.remainingMs;
    case State::Expired: return 0;
    default: return r_.intervalMs;
    }
}

float ChronoModel::fraction(int64_t now) const
{
    if (r_.intervalMs <= 0) {
        return 0.0f;
    }
    return float(double(remainingMs(now)) / double(r_.intervalMs));
}

int64_t ChronoModel::nextEventUtcMs() const { return r_.state == State::Running ? r_.endUtcMs : -1; }

} // namespace core
