// AeroDeck levelled serial logging. Never pass secret values to these functions (FR-013).
#pragma once
#include "core/LogFormat.h"

namespace Log {
void begin();
void setLevel(core::Lvl lvl);
core::Lvl level();
void logf(core::Lvl lvl, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
// Raw line output for serial replies (not filtered, not prefixed). Thread-safe.
void raw(const char* line);
} // namespace Log

#define LOGE(tag, ...) Log::logf(core::Lvl::E, tag, __VA_ARGS__)
#define LOGW(tag, ...) Log::logf(core::Lvl::W, tag, __VA_ARGS__)
#define LOGI(tag, ...) Log::logf(core::Lvl::I, tag, __VA_ARGS__)
#define LOGD(tag, ...) Log::logf(core::Lvl::D, tag, __VA_ARGS__)
