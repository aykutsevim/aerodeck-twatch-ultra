# Contract: Internal Module Interfaces (S0)

These are the C++ interfaces that later slices build on. Signatures are normative. Bodies
belong to implementation. Layering per AD-SAS-001 §3.

## Shared plain types (`src/core/Types.h`)
These live in `core/` so that host-tested core logic (e.g. `SimFail`) can use them. `src/board/IBoard.h` includes
this header. `core/` never includes `board/`.
```cpp
enum class Cap : uint32_t { Lora=1<<0, Haptics=1<<1, Speaker=1<<2, Mic=1<<3, IrTx=1<<4, Nfc=1<<5,
                            Gnss=1<<6, SdCard=1<<7, ImuStepCounter=1<<8, ImuWakeInterrupt=1<<9,
                            Amoled=1<<10 };
enum class Rail : uint8_t { Display, Lora, AudioAmp, Gnss, Nfc, Sensor, Count };
struct DisplayInfo { uint16_t w, h; bool round; };
struct PmuReading  { uint8_t battPct; int16_t battmA; int16_t dieTempC10; bool charging, vbus; };
// also: enum class Periph, enum class PeriphState, struct BootResult (see data-model.md §3)
```

## IBoard (`src/board/IBoard.h`)
```cpp
#include "core/Types.h"
class IBoard {
public:
  virtual BootResult begin(uint16_t simFailMask, bool safeMode, bool headless) = 0; // never aborts on optional HW; headless skips display/LVGL
  virtual uint32_t caps() const = 0;
  virtual const char* name() const = 0;
  virtual DisplayInfo display() const = 0;
  virtual void setBrightnessPct(uint8_t pct) = 0;
  virtual void setRail(Rail r, bool on) = 0;           // PowerService only
  virtual bool readPmu(PmuReading& out) = 0;           // takes I2C lock
  virtual void loop() = 0;                              // UI task; pumps vendor events
  virtual void onButtonClick(void (*cb)(void*), void* ctx) = 0;
  virtual PeriphState periph(Periph p) const = 0;
  virtual ~IBoard() = default;
};
IBoard& board();            // defined once per board in src/board/Board<Name>.cpp
I2cLock                      // RAII guard over the shared I2C mutex
```

## Page (`src/ui/Page.h`)
```cpp
class Page {
public:
  virtual PageId id() const = 0;
  virtual const char* title() const = 0;
  virtual void onCreate(lv_obj_t* screen) = 0;   // build widgets once
  virtual void onEnter() {}                       // acquire leases, start timers
  virtual void onExit() {}                        // MUST release all leases
  virtual void onTick(uint32_t dtMs) {}           // ≤10 Hz, UI task
  virtual ~Page() = default;
};
```

## PageManager (`src/ui/PageManager.h`)
```cpp
void registerPage(Page*);           // order of registration = ring order; Home first
void begin();                        // shows Home
void next(); void prev();            // swipe actions
void buttonPressed();                // FR-003 semantics
PageId current() const;
```

## SettingsStore (`src/services/SettingsStore.h`)
```cpp
bool begin();                                    // runs schema state machine
bool    getBool(SettingKey); uint8_t getU8(SettingKey); /* … per type */
SetResult set(SettingKey, Value);                 // validate → persist → emit SettingsChanged
void restoreDefaults();
const SettingDef& def(SettingKey);  size_t count();
```

## PowerService (`src/services/PowerService.h`)
```cpp
class RailLease { public: explicit RailLease(Rail); ~RailLease(); RailLease(RailLease&&); /* no copy */ };
const Snapshot<PowerSnapshot>& power();          // battPct, mA, dieTemp, charging; 1 Hz
```

## EventBus & Snapshot (`src/services/EventBus.h`, `Snapshot.h`)
```cpp
QueueHandle_t subscribe(uint8_t depth);           // per-subscriber queue
void publish(const Event&);                        // non-blocking, drops + counts on full
template<class T> class Snapshot { void write(const T&); T read() const; uint32_t seq() const; };
```

## Cautions (`src/services/Cautions.h`) (S0 minimal, superseded by AlertManager in S4)
```cpp
void raise(CautionId, const char* text); void clear(CautionId); size_t active(CautionId* out, size_t max);
```

## Log (`src/core/LogFormat.h` pure formatting/filtering + `src/services/Log.h` Serial sink)
```cpp
// core/LogFormat.h
enum class Lvl : uint8_t { E, W, I, D };
size_t formatLogLine(char* out, size_t n, uint32_t ms, Lvl, const char* tag, const char* msg);
bool   shouldLog(Lvl msg, Lvl threshold);
// services/Log.h
void logf(Lvl, const char* tag, const char* fmt, ...);   // no secrets by contract
```
