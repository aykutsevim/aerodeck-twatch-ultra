#include "services/HapticService.h"
#include <Arduino.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "board/IBoard.h"
#include "core/SettingDefs.h"
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"

using core::Pattern;
using core::Seq;

namespace HapticService {

namespace {

SemaphoreHandle_t s_mutex;
uint8_t s_curPrio;
uint32_t s_curEndMs;

const char* patternName(Pattern p)
{
    switch (p) {
    case Pattern::Click: return "click";
    case Pattern::Advisory: return "advisory";
    case Pattern::Caution: return "caution";
    case Pattern::StickShaker: return "shaker";
    default: return "none";
    }
}

enum class Outcome : uint8_t { Played, Suppressed, Dropped, Unavailable };

Outcome playImpl(Pattern p)
{
    const auto level = core::Intensity(SettingsStore::get(core::SettingKey::HapticLvl));
    const bool quiet = SettingsStore::getBool(core::SettingKey::Quiet);
    Seq seq;
    if (!core::HapticPolicy::select(p, level, quiet, seq)) {
        return Outcome::Suppressed;
    }
    if (!s_mutex) {
        return Outcome::Unavailable;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const uint32_t now = millis();
    if (!core::HapticPolicy::shouldStart(seq.prio, s_curPrio, s_curEndMs, now)) {
        xSemaphoreGive(s_mutex);
        LOGD("hap", "drop %s (prio %u playing)", patternName(p), s_curPrio);
        return Outcome::Dropped;
    }
    const bool ok = board().haptic(seq.w, seq.n);
    if (ok) {
        s_curPrio = seq.prio;
        s_curEndMs = now + seq.lengthMs;
    }
    xSemaphoreGive(s_mutex);
    if (!ok) {
        return Outcome::Unavailable;
    }
    LOGD("hap", "play %s prio=%u len=%u", patternName(p), seq.prio, seq.lengthMs);
    return Outcome::Played;
}

void cmdHaptic(int, const char* const* argv, core::Reply& r)
{
    const char* a = argv[0];
    if (strcmp(a, "stop") == 0) {
        if (s_mutex) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            board().hapticStop();
            s_curPrio = 0;
            s_curEndMs = millis();
            xSemaphoreGive(s_mutex);
        }
        r.ok();
        return;
    }
    Pattern p = strcmp(a, "click") == 0      ? Pattern::Click
                : strcmp(a, "advisory") == 0 ? Pattern::Advisory
                : strcmp(a, "caution") == 0  ? Pattern::Caution
                : strcmp(a, "shaker") == 0   ? Pattern::StickShaker
                                             : Pattern::None;
    if (p == Pattern::None) {
        r.err("expected click|advisory|caution|shaker|stop");
        return;
    }
    switch (playImpl(p)) {
    case Outcome::Played: r.line("OK played"); break;
    case Outcome::Suppressed:
        r.line(SettingsStore::get(core::SettingKey::HapticLvl) == 0 ? "OK suppressed (off)" : "OK suppressed (quiet)");
        break;
    case Outcome::Dropped: r.line("OK dropped (higher priority playing)"); break;
    case Outcome::Unavailable: r.err("haptics unavailable"); break;
    }
}

} // namespace

void begin()
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
    SerialConsole::add("haptic", 1, cmdHaptic);
}

bool play(Pattern p) { return playImpl(p) == Outcome::Played; }

void click() { playImpl(Pattern::Click); }

void stopWarning()
{
    if (!s_mutex) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_curPrio == core::HapticPolicy::priority(Pattern::StickShaker) && int32_t(millis() - s_curEndMs) < 0) {
        board().hapticStop();
        LOGD("hap", "stop shaker (ack)");
    }
    if (s_curPrio == core::HapticPolicy::priority(Pattern::StickShaker)) {
        s_curPrio = 0;
        s_curEndMs = millis();
    }
    xSemaphoreGive(s_mutex);
}

} // namespace HapticService
