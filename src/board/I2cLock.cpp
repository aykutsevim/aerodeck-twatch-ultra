#include "board/I2cLock.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t mutex()
{
    static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
    return m;
}

I2cLock::I2cLock() { xSemaphoreTakeRecursive(mutex(), portMAX_DELAY); }
I2cLock::~I2cLock() { xSemaphoreGiveRecursive(mutex()); }
