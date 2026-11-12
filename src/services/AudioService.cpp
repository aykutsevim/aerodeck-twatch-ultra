#include "services/AudioService.h"
#include <Arduino.h>
#include <FFat.h>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "board/IBoard.h"
#include "core/AudioMath.h"
#include "core/AudioPolicy.h"
#include "core/SettingDefs.h"
#include "core/Wav.h"
#include "services/Log.h"
#include "services/PowerService.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include "services/TimeService.h"

using core::Clip;
using core::SettingKey;
using core::SoundPrio;
using core::SoundQueue;
using core::SoundReq;

namespace AudioService {

namespace {

// Tuning (research R4; measured values in specs/006-gpws-audio/validation.md).
constexpr uint32_t kSettleMs = 30;         // amplifier rail on → first sample
constexpr uint32_t kGuardMs = 20;          // zeros before the first clip and after each clip (pop guard)
constexpr size_t kFlushSamples = 4096;     // zeros after the last clip: pushes the clip's tail out of the DMA ring
constexpr uint32_t kTailMs = 150;          // wait for a follow-up sound before the amplifier goes off
constexpr size_t kChunk = 512;             // samples per write (32 ms at 16 kHz)
constexpr const char* kDir = "/audio/";

portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
SoundQueue s_queue;                       // guarded by s_lock
volatile SoundPrio s_playingPrio = SoundPrio::None;
volatile Clip s_playing = Clip::None;
volatile bool s_abort;                    // stop the clip in progress (stop / preempt)
volatile bool s_ampOn;
volatile uint32_t s_played;
volatile uint32_t s_skipped;
uint32_t s_skipLogged;                    // bit per clip: "skip" already logged this boot (audio task)
bool s_outputLogged;
TaskHandle_t s_task;
int16_t s_buf[kChunk];                    // audio task only (internal RAM)

// Chime scheduler state (svc task).
int64_t s_lastChimeHour = -1;
int64_t s_lastSec = -1;

const char* prioName(SoundPrio p)
{
    switch (p) {
    case SoundPrio::Warning: return "warning";
    case SoundPrio::Caution: return "caution";
    case SoundPrio::Chime: return "chime";
    case SoundPrio::Sample: return "sample";
    default: return "-";
    }
}

uint8_t volume() { return uint8_t(SettingsStore::get(SettingKey::Volume)); }

bool localNow(int64_t& localSec)
{
    const ClockSnapshot c = TimeService::clock().read();
    localSec = c.utc + int64_t(c.tzOffsetMin) * 60;
    return c.valid;
}

bool inQuietHoursNow()
{
    int64_t t;
    if (!localNow(t)) {
        return false;   // unknown time: quiet hours cannot apply
    }
    return core::inQuietHours(core::localHourOf(t), uint8_t(SettingsStore::get(SettingKey::QhStart)),
                              uint8_t(SettingsStore::get(SettingKey::QhEnd)));
}

void path(Clip c, char* out, size_t n) { snprintf(out, n, "%s%s", kDir, core::clipFile(c)); }

// Opens and validates a clip. On success the file is positioned at the first sample.
enum class Open : uint8_t { Ok, Missing, Bad };
Open openClip(Clip c, fs::File& f, core::Wav::Info& w)
{
    char p[40];
    path(c, p, sizeof p);
    f = FFat.open(p, "r");
    if (!f) {
        return Open::Missing;
    }
    uint8_t hdr[core::Wav::kHeaderMax];
    const size_t n = f.read(hdr, sizeof hdr);
    if (!core::Wav::parse(hdr, n, w) || w.dataOffset + w.dataBytes > f.size() + 0u || !f.seek(w.dataOffset)) {
        f.close();
        return Open::Bad;
    }
    return Open::Ok;
}

void writeZeros(size_t samples)
{
    memset(s_buf, 0, sizeof s_buf);
    while (samples > 0) {
        const size_t k = samples < kChunk ? samples : kChunk;
        board().audioWrite(s_buf, k);
        samples -= k;
    }
}

size_t samplesFor(uint32_t ms, uint32_t rate) { return size_t(uint64_t(rate) * ms / 1000u); }

// Streams one clip. Returns false if it was aborted (stop / preempt).
bool stream(fs::File& f, const core::Wav::Info& w)
{
    uint32_t left = w.dataBytes / 2;
    while (left > 0) {
        if (s_abort) {
            return false;
        }
        const size_t want = left < kChunk ? left : kChunk;
        const size_t got = f.read(reinterpret_cast<uint8_t*>(s_buf), want * 2) / 2;
        if (got == 0) {
            break;   // short file: play what there was
        }
        core::AudioMath::applyGain(s_buf, got, core::AudioMath::gainQ15(volume()));
        board().audioWrite(s_buf, got);
        left -= uint32_t(got);
    }
    return true;
}

bool popNext(SoundReq& r)
{
    portENTER_CRITICAL(&s_lock);
    const bool ok = s_queue.pop(millis(), r);
    if (ok) {
        s_playing = r.clip;
        s_playingPrio = r.prio;
        s_abort = false;
    }
    portEXIT_CRITICAL(&s_lock);
    return ok;
}

void setIdle()
{
    portENTER_CRITICAL(&s_lock);
    s_playing = Clip::None;
    s_playingPrio = SoundPrio::None;
    portEXIT_CRITICAL(&s_lock);
}

void dropQueue()
{
    portENTER_CRITICAL(&s_lock);
    const size_t n = s_queue.size();
    s_queue.clear();
    portEXIT_CRITICAL(&s_lock);
    s_skipped += uint32_t(n);
}

void logSkip(Clip c, const char* why)
{
    const uint32_t bit = 1u << uint8_t(c);
    if (!(s_skipLogged & bit)) {
        s_skipLogged |= bit;
        LOGW("aud", "skip %s: %s", core::clipName(c), why);
    }
}

void audioTask(void*)
{
    RailLease* lease = nullptr;
    uint32_t onMs = 0;
    uint32_t lastEndMs = 0;
    uint32_t rate = 0;
    for (;;) {
        SoundReq r;
        if (!popNext(r)) {
            if (!lease) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                continue;
            }
            // Last clip done: flush its tail out of the DMA ring, give a follow-up sound a short chance, then off.
            writeZeros(kFlushSamples);
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kTailMs)) > 0) {
                portENTER_CRITICAL(&s_lock);
                const bool more = s_queue.size() > 0;
                portEXIT_CRITICAL(&s_lock);
                if (more) {
                    continue;
                }
            }
            board().audioEnd();
            delete lease;
            lease = nullptr;
            s_ampOn = false;
            rate = 0;
            LOGI("aud", "amp OFF after %lu ms (on %lu ms)", (unsigned long)(millis() - lastEndMs),
                 (unsigned long)(millis() - onMs));
            continue;
        }

        fs::File f;
        core::Wav::Info w;
        const Open o = openClip(r.clip, f, w);
        if (o != Open::Ok) {
            s_skipped++;
            logSkip(r.clip, o == Open::Missing ? "missing" : "bad format");
            setIdle();
            continue;
        }
        if (w.rate != rate) {
            if (!board().audioBegin(w.rate)) {
                f.close();
                s_skipped++;
                dropQueue();
                if (!s_outputLogged) {
                    s_outputLogged = true;
                    LOGW("aud", "output unavailable (speaker %s)", core::periphStateName(board().periph(core::Periph::Speaker)));
                }
                setIdle();
                if (lease) {
                    delete lease;
                    lease = nullptr;
                    s_ampOn = false;
                }
                rate = 0;
                continue;
            }
            rate = w.rate;
        }
        if (!lease) {
            lease = new RailLease(core::Rail::AudioAmp);
            s_ampOn = true;
            onMs = millis();
            LOGI("aud", "amp ON");
            vTaskDelay(pdMS_TO_TICKS(kSettleMs));
            writeZeros(samplesFor(kGuardMs, rate));
        }
        LOGI("aud", "play %s prio=%u ms=%lu", core::clipName(r.clip), unsigned(r.prio),
             (unsigned long)core::Wav::durationMs(w));
        const bool done = stream(f, w);
        f.close();
        if (done) {
            writeZeros(samplesFor(kGuardMs, rate));
            s_played++;
        } else {
            board().audioEnd();   // cut now: clear what is still queued in the DMA ring
        }
        lastEndMs = millis();
        setIdle();
    }
}

PlayResult playImpl(Clip c, SoundPrio p, const char** reason)
{
    if (!core::clipName(c)) {
        return PlayResult::NoClip;
    }
    const char* deny = core::denyReason(p, SettingsStore::getBool(SettingKey::Quiet), volume(), inQuietHoursNow());
    if (deny) {
        if (reason) *reason = deny;
        LOGD("aud", "suppressed %s (%s)", core::clipName(c), deny);
        return PlayResult::Suppressed;
    }
    if (!s_task) {
        return PlayResult::Full;
    }
    portENTER_CRITICAL(&s_lock);
    const core::PushResult pr = s_queue.push(SoundReq{c, p, millis()});
    if (pr == core::PushResult::Queued && s_playingPrio != SoundPrio::None && core::shouldPreempt(s_playingPrio, p)) {
        s_abort = true;   // a more important sound cuts the one playing (FR-002)
    }
    portEXIT_CRITICAL(&s_lock);
    if (pr == core::PushResult::Duplicate) return PlayResult::Duplicate;
    if (pr == core::PushResult::Full) return PlayResult::Full;
    xTaskNotifyGive(s_task);
    return PlayResult::Queued;
}

// ---- chime (US3) ----

core::ChimeDecision chimeAt(int64_t localSec, bool valid)
{
    return core::chimeDue(localSec, valid, SettingsStore::getBool(SettingKey::Chime),
                          SettingsStore::getBool(SettingKey::Quiet), volume(),
                          uint8_t(SettingsStore::get(SettingKey::QhStart)),
                          uint8_t(SettingsStore::get(SettingKey::QhEnd)), s_lastChimeHour);
}

// ---- serial commands (contracts/serial_commands.md) ----

void replyPlay(PlayResult pr, const char* why, core::Reply& r)
{
    char b[48];
    switch (pr) {
    case PlayResult::Queued: r.line("OK queued"); break;
    case PlayResult::Suppressed:
        snprintf(b, sizeof b, "OK suppressed (%s)", why ? why : "?");
        r.line(b);
        break;
    case PlayResult::Duplicate: r.line("OK duplicate"); break;
    case PlayResult::Full: r.err("queue full"); break;
    case PlayResult::NoClip: r.err("unknown clip"); break;
    }
}

void cmdStatus(int, const char* const*, core::Reply& r)
{
    const AudioStatus s = status();
    char b[120];
    snprintf(b, sizeof b, "amp=%s playing=%s queued=%u played=%lu skipped=%lu volume=%u chime=%d qh=%d-%d quiet=%d",
             s.ampOn ? "ON" : "OFF", s.playing == Clip::None ? "-" : core::clipName(s.playing), unsigned(s.queued),
             (unsigned long)s.played, (unsigned long)s.skipped, unsigned(volume()),
             SettingsStore::getBool(SettingKey::Chime) ? 1 : 0, int(SettingsStore::get(SettingKey::QhStart)),
             int(SettingsStore::get(SettingKey::QhEnd)), SettingsStore::getBool(SettingKey::Quiet) ? 1 : 0);
    r.ok();
    r.line(b);
}

void cmdPlay(int, const char* const* argv, core::Reply& r)
{
    Clip c;
    if (!core::clipFromName(argv[0], c)) {
        r.err("unknown clip");
        return;
    }
    const char* why = nullptr;
    replyPlay(playImpl(c, SoundPrio::Sample, &why), why, r);
}

void cmdStop(int, const char* const*, core::Reply& r)
{
    stop(SoundPrio::Sample, "cmd");
    r.ok();
}

void cmdList(int, const char* const*, core::Reply& r)
{
    r.ok();
    char b[64];
    for (uint8_t i = 1; i < uint8_t(Clip::Count); i++) {
        const Clip c = Clip(i);
        fs::File f;
        core::Wav::Info w;
        switch (openClip(c, f, w)) {
        case Open::Ok:
            snprintf(b, sizeof b, "%s ok %lu.%02lus %luHz", core::clipName(c), (unsigned long)(core::Wav::durationMs(w) / 1000),
                     (unsigned long)(core::Wav::durationMs(w) % 1000 / 10), (unsigned long)w.rate);
            f.close();
            break;
        case Open::Missing: snprintf(b, sizeof b, "%s MISSING", core::clipName(c)); break;
        case Open::Bad: snprintf(b, sizeof b, "%s BAD (format)", core::clipName(c)); break;
        }
        r.line(b);
    }
}

void cmdChimeCheck(int, const char* const* argv, core::Reply& r)
{
    int h, m, s;
    if (sscanf(argv[0], "%d:%d:%d", &h, &m, &s) != 3 || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        r.err("expected HH:MM:SS");
        return;
    }
    int64_t now;
    const bool valid = localNow(now);
    // today's local date (floor to the day) + the given time; the real last-chimed hour (spec U2)
    int64_t day = now / 86400;
    if (now < 0 && now % 86400) day--;
    const core::ChimeDecision d = chimeAt(day * 86400 + h * 3600 + m * 60 + s, valid);
    char b[48];
    snprintf(b, sizeof b, "OK due=%d reason=%s", d.due ? 1 : 0, core::chimeReasonName(d.reason));
    r.line(b);
}

void cmdChimeNow(int, const char* const*, core::Reply& r)
{
    const char* why = nullptr;
    const PlayResult pr = playImpl(Clip::Chime, SoundPrio::Chime, &why);
    if (pr == PlayResult::Queued) {
        r.ok();
        return;
    }
    replyPlay(pr, why, r);
}

} // namespace

void begin()
{
    if (!s_task) {
        // core 0, prio 5 (AD-SAS-001 §5.1); 4 KB stack: the sample buffer is static
        xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, &s_task, 0);
    }
    SerialConsole::add("audio", 0, cmdStatus);
    SerialConsole::add("audio play", 1, cmdPlay);
    SerialConsole::add("audio stop", 0, cmdStop);
    SerialConsole::add("audio list", 0, cmdList);
    SerialConsole::add("audio chimecheck", 1, cmdChimeCheck);
    SerialConsole::add("audio chimenow", 0, cmdChimeNow);
}

void tick(uint32_t)
{
    int64_t t;
    const bool valid = localNow(t);
    if (t == s_lastSec) {
        return;   // evaluate once per second
    }
    s_lastSec = t;
    const core::ChimeDecision d = chimeAt(t, valid);
    if (!d.due) {
        return;
    }
    s_lastChimeHour = d.hourKey;   // one decision per hour, even if the sound is then suppressed or dropped
    const PlayResult pr = playImpl(Clip::Chime, SoundPrio::Chime, nullptr);
    LOGI("aud", "chime hh=%02d%s", core::localHourOf(t), pr == PlayResult::Queued ? "" : " (not queued)");
}

PlayResult play(Clip c, SoundPrio p, const char** reason) { return playImpl(c, p, reason); }

void stop(SoundPrio minPrio, const char* why)
{
    portENTER_CRITICAL(&s_lock);
    const size_t before = s_queue.size();
    s_queue.purge(minPrio);
    const bool purged = s_queue.size() != before;
    const bool cut = s_playingPrio != SoundPrio::None && uint8_t(s_playingPrio) >= uint8_t(minPrio);
    if (cut) {
        s_abort = true;
    }
    portEXIT_CRITICAL(&s_lock);
    if (cut || purged) {
        LOGI("aud", "stop (%s)", why);
    }
}

void stopAlerts() { stop(SoundPrio::Caution, "ack"); }

AudioStatus status()
{
    AudioStatus s;
    portENTER_CRITICAL(&s_lock);
    s.ampOn = s_ampOn;
    s.playing = s_playing;
    s.queued = uint8_t(s_queue.size());
    s.played = s_played;
    s.skipped = s_skipped;
    portEXIT_CRITICAL(&s_lock);
    return s;
}

} // namespace AudioService
