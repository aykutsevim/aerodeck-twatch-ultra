#include "services/AttitudeService.h"
#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "board/IBoard.h"
#include "core/Attitude.h"
#include "core/LockDetector.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/SerialConsole.h"

namespace AttitudeService {

// Watch-frame mapping of the BHI260AP axes (after LilyGoLib's TOP_LAYER_BOTTOM_RIGHT_CORNER remap).
// Determined on the device in spike T014 (see specs/002-pfd-watch-face/validation.md).
static constexpr core::AxisMap kAxes = core::kIdentityAxes;

static Snapshot<AttitudeSnapshot> s_snap;
static core::Attitude s_att;
static core::LockDetector s_lock;
static volatile AttStatus s_status = AttStatus::Align;
static volatile uint32_t s_lastSampleMs;
static volatile uint32_t s_samples;      // samples since last rate window
static volatile uint32_t s_rateHz;
static uint32_t s_seq;
static uint32_t s_rateWindowMs;
static volatile uint32_t s_latLast, s_latMax, s_latMaxWindowStart;
static volatile float s_raw[3];
static volatile bool s_sweep;   // test hook `ad att sweep`: synthetic motion for FPS measurement (SC-001)

static void publish(float pitch, float roll, uint32_t ms)
{
    AttitudeSnapshot a;
    a.pitchDeg = pitch;
    a.rollDeg = roll;
    a.status = s_status;
    a.level = s_lock.update(pitch, roll, a.status == AttStatus::Ok, ms);
    a.sampleMs = ms;
    a.seq = ++s_seq;
    s_snap.write(a);
}

static void setStatus(AttStatus st)
{
    if (s_status == st) {
        return;
    }
    s_status = st;
    AttitudeSnapshot a = s_snap.read();
    a.status = st;
    if (st != AttStatus::Ok) {
        a.level = false;
        a.pitchDeg = 0;   // level picture while aligning/failed (FR-005)
        a.rollDeg = 0;
    }
    a.seq = ++s_seq;
    s_snap.write(a);
    EventBus::publish(EventType::AttitudeStatusChanged, uint32_t(st));
    LOGI("att", "status %s", st == AttStatus::Ok ? "OK" : st == AttStatus::Align ? "ALIGN" : "FAIL");
}

// UI task, inside board().loop() with the I2C lock held: math + snapshot only.
static void onGravity(const GravitySample& g, void*)
{
    s_raw[0] = g.x;
    s_raw[1] = g.y;
    s_raw[2] = g.z;
    s_lastSampleMs = g.ms;
    s_samples++;
    if (s_status != AttStatus::Ok) {
        return;
    }
    if (s_sweep) {
        // Roll ±30° at 0.5 Hz, pitch ±10° at 0.3 Hz: the horizon changes on every sample.
        const float t = g.ms / 1000.0f;
        publish(10.0f * std::sin(t * 1.885f), 30.0f * std::sin(t * 3.1416f), g.ms);
        return;
    }
    core::AttitudeOut o = s_att.update(g.x, g.y, g.z);
    publish(o.pitchDeg, o.rollDeg, g.ms);
}

static const char* statusName(AttStatus s)
{
    return s == AttStatus::Ok ? "OK" : s == AttStatus::Align ? "ALIGN" : "FAIL";
}

static void cmdAtt(int, const char* const*, core::Reply& r)
{
    AttitudeSnapshot a = s_snap.read();
    char b[160];
    r.ok();
    snprintf(b, sizeof b, "status=%s pitch=%.1f roll=%.1f lock=%d rate_hz=%lu lat_ms=%lu lat_max_ms=%lu",
             statusName(a.status), a.pitchDeg, a.rollDeg, a.level ? 1 : 0, (unsigned long)s_rateHz,
             (unsigned long)s_latLast, (unsigned long)s_latMax);
    r.line(b);
    snprintf(b, sizeof b, "raw_g=%.3f,%.3f,%.3f", s_raw[0], s_raw[1], s_raw[2]);
    r.line(b);
}

static void cmdSweep(int, const char* const* argv, core::Reply& r)
{
    if (strcmp(argv[0], "on") == 0) {
        s_sweep = true;
    } else if (strcmp(argv[0], "off") == 0) {
        s_sweep = false;
    } else {
        r.err("expected on|off");
        return;
    }
    r.ok();
}

void begin()
{
    SerialConsole::add("att sweep", 1, cmdSweep);
    s_att.setAxisMap(kAxes);
    AttitudeSnapshot a{0, 0, false, AttStatus::Align, 0, 0};
    s_snap.write(a);
    SerialConsole::add("att", 0, cmdAtt);
}

void onImuReady(bool imuOk)
{
    if (!imuOk) {
        setStatus(AttStatus::Fail);
        return;
    }
    s_att.reset();
    bool ok = board().configureAttitude(kRateHz, onGravity, nullptr);
    if (!ok) {
        LOGW("att", "gravity sensor configuration failed");
    }
    s_lastSampleMs = millis();
    setStatus(ok ? AttStatus::Ok : AttStatus::Fail);
}

const Snapshot<AttitudeSnapshot>& att() { return s_snap; }

void tick(uint32_t now)
{
    if (now - s_rateWindowMs >= 1000) {
        s_rateHz = s_samples;
        s_samples = 0;
        s_rateWindowMs = now;
    }
    const int32_t sinceSample = int32_t(now - s_lastSampleMs);   // may be negative: now was taken before
    if (s_status == AttStatus::Ok && sinceSample > 1000) {
        LOGW("att", "no gravity samples for >1 s");
        setStatus(AttStatus::Fail);
    } else if (s_status == AttStatus::Fail && sinceSample < 200 && s_rateHz > 10) {
        s_att.reset();
        setStatus(AttStatus::Ok);   // samples resumed
    }
}

void notifyRendered(uint32_t sampleMs)
{
    uint32_t now = millis();
    uint32_t lat = now - sampleMs;
    s_latLast = lat;
    if (now - s_latMaxWindowStart > 10000) {
        s_latMax = 0;
        s_latMaxWindowStart = now;
    }
    if (lat > s_latMax) {
        s_latMax = lat;
    }
}

} // namespace AttitudeService
