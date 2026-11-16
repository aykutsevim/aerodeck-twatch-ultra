# Contract: IBoard Radio API (S9)

Only `src/board/` implements these. Calls are made from the `radio` task (core 0), except `radioAirtimeMs`, which
may be called from any task.

```cpp
struct RadioProfile { float freqMHz; float bwKHz; uint8_t sf; uint8_t crDenom; int8_t powerDbm;
                      uint8_t syncWord; uint16_t preamble; };
struct RadioPacket  { uint8_t data[64]; uint8_t len; int16_t rssi; int8_t snrQ4; bool crcError; };

// The rail must already be on (RailLease(Lora) held by TcasService). Configures the SX1262, selects the built-in
// antenna (setRFSwitch(false)), installs the DIO1 ISR, enters continuous RX. false = no radio / failed.
virtual bool radioStart(const RadioProfile& p) = 0;
virtual void radioStop() = 0;                                 // sleep the radio, detach the ISR (before rail off)
// Blocking transmit (≈ airtime), then back to RX. Returns false on failure; airMs = measured time on air.
virtual bool radioSend(const uint8_t* buf, size_t n, uint32_t& airMs) = 0;
// Non-blocking: true when a packet (or a CRC error) is available since the last call. RX restarts automatically.
virtual bool radioPoll(RadioPacket& out) = 0;
virtual uint32_t radioAirtimeMs(size_t n) = 0;                // RadioLib getTimeOnAir for the current profile
virtual uint32_t nodeId() const = 0;                          // low 32 bits of the eFuse MAC
// Waits (≤ timeoutMs) for the DIO1 notification; used by the radio task to sleep between events.
virtual bool radioWait(uint32_t timeoutMs) = 0;
```

**T-Watch S3:** `radioStart` returns false (no LoRa in S0 caps); `nodeId` works.

**SPI:** every radio call takes LilyGoLib's `lockSPI()` (the bus is shared with SD and NFC).
