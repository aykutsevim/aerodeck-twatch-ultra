#include "services/Log.h"
#include <Arduino.h>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace Log {

static SemaphoreHandle_t s_mutex;
static volatile core::Lvl s_level = core::Lvl::I;

void begin()
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
}

void setLevel(core::Lvl lvl) { s_level = lvl; }
core::Lvl level() { return s_level; }

static void emit(const char* line)
{
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
    Serial.println(line);
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

void logf(core::Lvl lvl, const char* tag, const char* fmt, ...)
{
    if (!core::shouldLog(lvl, s_level)) {
        return;
    }
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    char line[200];
    core::formatLogLine(line, sizeof line, millis(), lvl, tag, msg);
    emit(line);
}

void raw(const char* line)
{
    emit(line);
    // Serial replies come only from the svc task (core 0); flushing per line stops the USB-CDC
    // driver from dropping lines in multi-line replies such as `ad periph`.
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
    Serial.flush();
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

} // namespace Log
