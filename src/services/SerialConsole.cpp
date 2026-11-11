#include "services/SerialConsole.h"
#include <Arduino.h>
#include <esp_system.h>
#include "board/IBoard.h"
#include "services/Log.h"
#include "services/SvcTask.h"

#ifndef AD_FW_VERSION
#define AD_FW_VERSION "dev"
#endif

namespace SerialConsole {

static core::CommandParser s_parser;
static char s_line[core::CommandParser::kMaxLine + 1];
static size_t s_len;
static bool s_overflow;
static bool s_safe;

namespace {
struct SerialReply : core::Reply {
    void ok() override { Log::raw("OK"); }
    void err(const char* r) override
    {
        char b[96];
        snprintf(b, sizeof b, "ERR %s", r);
        Log::raw(b);
    }
    void line(const char* t) override { Log::raw(t); }
};
} // namespace

static const char* resetReason()
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
    }
}

static void cmdHelp(int, const char* const*, core::Reply& r)
{
    r.ok();
    char b[64];
    for (size_t i = 0; i < s_parser.count(); i++) {
        snprintf(b, sizeof b, "ad %s", s_parser.pattern(i));
        r.line(b);
    }
}

static void cmdInfo(int, const char* const*, core::Reply& r)
{
    char b[96];
    r.ok();
    snprintf(b, sizeof b, "fw=%s", AD_FW_VERSION);
    r.line(b);
    snprintf(b, sizeof b, "board=%s", board().name());
    r.line(b);
    core::DisplayInfo di = board().display();
    snprintf(b, sizeof b, "display=%ux%u corner_r=%u", di.w, di.h, di.cornerR);
    r.line(b);
    snprintf(b, sizeof b, "uptime_s=%lu", (unsigned long)(millis() / 1000));
    r.line(b);
    snprintf(b, sizeof b, "reset_reason=%s", resetReason());
    r.line(b);
    snprintf(b, sizeof b, "safe_mode=%d", s_safe ? 1 : 0);
    r.line(b);
}

static void cmdReboot(int, const char* const*, core::Reply& r)
{
    r.ok();
    Log::raw("");
    Serial.flush();
    delay(50);
    esp_restart();
}

bool add(const char* pattern, uint8_t minArgs, core::Handler fn) { return s_parser.add(pattern, minArgs, fn); }

void setSafeModeFlag(bool safe) { s_safe = safe; }

void tick(uint32_t)
{
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) {
            break;
        }
        if (c == '\n' || c == '\r') {
            if (s_len == 0 && !s_overflow) {
                continue;
            }
            SerialReply reply;
            if (s_overflow) {
                reply.err("line too long");
            } else {
                s_line[s_len] = '\0';
                s_parser.dispatch(s_line, reply);
            }
            Log::raw("");   // reply terminator (blank line)
            s_len = 0;
            s_overflow = false;
            continue;
        }
        if (s_len < core::CommandParser::kMaxLine) {
            s_line[s_len++] = char(c);
        } else {
            s_overflow = true;
        }
    }
}

void begin()
{
    add("help", 0, cmdHelp);
    add("info", 0, cmdInfo);
    add("reboot", 0, cmdReboot);
    SvcTask::addTick(tick);
}

} // namespace SerialConsole
