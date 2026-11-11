# Contract: Internal Interface Changes (S1)

## IBoard additions (`src/board/IBoard.h`)
```cpp
struct GravitySample { float x, y, z; uint32_t ms; };     // in g, sensor frame after LilyGoLib remap
using GravityCb = void (*)(const GravitySample&, void* ctx);
virtual bool configureAttitude(float rateHz, GravityCb cb, void* ctx) = 0;  // BHI GRA; false if IMU not Ok
virtual bool readRtc(std::tm& utc, bool& integrity) = 0;  // I2C lock
virtual bool writeRtc(const std::tm& utc) = 0;            // I2C lock
```
`BoardS3`: `configureAttitude` returns false (BMA423 has no fusion; accel fallback out of scope). RTC is
implemented with the PCF8563.

## Core (pure, host-tested)
```cpp
namespace core {
struct AttitudeOut { float pitchDeg, rollDeg; };
class Attitude { public: AttitudeOut update(float gx, float gy, float gz); void reset(); };
class LockDetector { public: bool update(float pitch, float roll, bool statusOk, uint32_t nowMs); };
struct LocalTime { uint8_t h, m, s; float headingDeg; };
LocalTime splitLocal(int64_t utcSec, uint16_t ms, int16_t tzOffsetMin);
bool parseIso8601Utc(const char* s, int64_t& outUtc);
bool clockPlausible(int year);
class FuelFlow { public: void addSample(uint32_t minute, uint8_t pct, bool charging); float ratePctPerH() const; void reset(); };
class FuelState { public: bool update(uint8_t pct, bool charging); };   // returns lowFuel
}
```

## Services
```cpp
namespace AttitudeService { void begin(); const Snapshot<AttitudeSnapshot>& att(); void onImuReady(bool ok); }
namespace TimeService { void begin(); const Snapshot<ClockSnapshot>& clock(); bool set(int64_t utc); }
// PowerService: PowerSnapshot gains fuelFlowPctPerH (NaN = n/a) and lowFuel; raises/clears LOW FUEL.
```

## UI widgets (`src/ui/widgets/`)
```cpp
class AdiWidget   { create(lv_obj_t* parent, int32_t size); void set(const AttitudeSnapshot&); };  // DRAW_MAIN custom draw
class TapeWidget  { enum Dir { Vertical, Horizontal }; create(parent, Dir, w, h, valueFmt); void setValue(float v); };
class EicasWidget { create(parent, w, h); void set(const PowerSnapshot&); };
```

## Pages
`PfdPage` replaces `HomePage` as `PageId::Home`. `onEnter` creates a 16 ms frame `lv_timer`; `onExit` deletes it.
