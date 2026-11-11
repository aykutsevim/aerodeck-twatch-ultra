// Shared I2C bus lock (touch, IMU, PMU, RTC, haptics and expander share one bus across both cores).
#pragma once

class I2cLock {
public:
    I2cLock();
    ~I2cLock();
    I2cLock(const I2cLock&) = delete;
    I2cLock& operator=(const I2cLock&) = delete;
};
