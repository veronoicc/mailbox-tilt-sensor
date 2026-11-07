#include "cmd/serial_cli.h"
#include "settings.h"

#if ENABLE_SERIAL_CLI
#include <Arduino.h>
#include "cmd/cmd.h"
#include "core/power.h"

static constexpr size_t MAX_LINE = 64;  // serial input is untrusted too; bound the buffer.

static char g_line[MAX_LINE + 1];
static size_t g_len = 0;
static bool g_overflow = false;
static bool g_started = false;

static void serialWrite(void*, const char* line) { Serial.println(line); }
static const CmdOut kSerialOut = {serialWrite, nullptr};
#endif

void serialCliBegin() {
#if ENABLE_SERIAL_CLI
  if (g_started || !usbConnected()) return;
  if (!Serial) Serial.begin(115200);
  g_started = true;
  g_len = 0;
  g_overflow = false;
#endif
}

void serialCliPoll() {
#if ENABLE_SERIAL_CLI
  if (!g_started) return;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_overflow) {
        serialWrite(nullptr, "{\"status\":\"ERROR\",\"msg\":\"Line too long\"}");
      } else if (g_len > 0) {
        g_line[g_len] = 0;
        cmdRunLine(g_line, CmdSource::Serial, kSerialOut);
      }
      g_len = 0;
      g_overflow = false;
    } else if (g_len < MAX_LINE) {
      g_line[g_len++] = c;
    } else {
      g_overflow = true;
    }
  }
#endif
}
