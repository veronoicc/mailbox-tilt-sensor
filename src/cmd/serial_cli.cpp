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

static void serialReply(void*, bool ok, const char* msg) { Serial.printf("[%s] %s\n", ok ? "OK" : "ERROR", msg); }

static void serialStatus(void*, StatusComp comp) {
  char line[160];
  statusText(comp, line, sizeof line);
  Serial.println(line);
}

static const CmdOut kSerialOut = {serialReply, serialStatus, nullptr};
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
        serialReply(nullptr, false, "Line too long");
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
