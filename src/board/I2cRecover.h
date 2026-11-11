// I2C bus recovery + PMU presence probe (board layer only).
// A soft reset (`ad reboot`, esp_restart, panic) can land in the middle of an I2C transfer. The slave that was
// sending then keeps SDA low, and the next boot's first transaction fails ("PMU not found"). Observed on the
// T-Watch Ultra during S2 validation (rapid reboots). Standard recovery (I2C spec §3.1.16 / NXP UM10204): clock
// SCL up to 9 times until the slave releases SDA, then generate a STOP.
#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace i2c {

inline void busRecover(int sda, int scl)
{
    Wire.end();
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, OUTPUT_OPEN_DRAIN);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    for (int i = 0; i < 9 && digitalRead(sda) == LOW; ++i) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
    }
    // STOP: SDA low -> high while SCL is high.
    pinMode(sda, OUTPUT_OPEN_DRAIN);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
}

// Returns true when `addr` ACKs. Recovers the bus before every attempt; leaves Wire started on success.
inline bool probeWithRecovery(int sda, int scl, uint8_t addr, int attempts = 3)
{
    for (int a = 0; a < attempts; ++a) {
        busRecover(sda, scl);
        Wire.begin(sda, scl);
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (a > 0) log_w("I2C: device 0x%02x answered after %d recovery attempt(s)", addr, a + 1);
            return true;
        }
        delay(20);
    }
    return false;
}

}  // namespace i2c
