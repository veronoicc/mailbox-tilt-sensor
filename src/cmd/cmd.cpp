// Command table and dispatcher. Adding a command = adding one row to kCommands.
#include "cmd/cmd.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "settings.h"
#include "ble/console.h"
#include "core/log.h"
#include "core/power.h"
#include "core/state.h"
#include "lora/lorawan.h"
#include "lora/uplink.h"
#include "motion/imu.h"

typedef void (*CmdHandler)(const ParsedLine& args, CmdSource src, const CmdOut& out);
struct CmdDef {
  const char* name;
  uint8_t maxArgs;
  uint8_t downlinkOpcode;  // 0 = not reachable by downlink
  CmdHandler handler;
};

static constexpr uint8_t MAX_DEPTH = 3;       // handlers that transmit may receive more downlinks
static constexpr uint8_t MAX_DRAIN_FRAMES = 4;

static bool g_sleepRequested = false;
static uint8_t g_depth = 0;

static void reply(const CmdOut& out, bool ok, const char* msg) { out.reply(out.ctx, ok, msg); }

static void cmdRecalibrate(const ParsedLine&, CmdSource, const CmdOut& out) {
  float zero = imuCalibrateZero();
  DBG_PRINTF("[CALIBRATION] New zero horizon offset: %.2f deg\n", zero);
  reply(out, true, "Zero horizon calibrated");
  out.status(out.ctx, StatusComp::Imu);
}

static void cmdTestUplink(const ParsedLine&, CmdSource, const CmdOut& out) {
  reply(out, true, "Triggering LoRaWAN test uplink...");
  // peak = current tilt, rest = zero offset so the console can show it.
  bool ok = lorawanSend(UplinkMsg::manualTest(imuRead().pitch, imuTiltZero()));
  reply(out, ok, ok ? "LoRaWAN test packet finished" : "LoRaWAN test packet failed");
  out.status(out.ctx, StatusComp::Lora);
}

static void cmdResetKnocks(const ParsedLine&, CmdSource, const CmdOut& out) {
  bleConsoleResetKnocks();
  reply(out, true, "Knock counter reset to zero");
  out.status(out.ctx, StatusComp::Knock);
}

static void cmdDfu(const ParsedLine&, CmdSource, const CmdOut& out) {
  reply(out, true, "Rebooting to UF2 Bootloader...");
  delay(300);
  enterUf2Dfu();
}

static void cmdSleep(const ParsedLine&, CmdSource, const CmdOut& out) {
  reply(out, true, "Entering Deep Sleep...");
  delay(300);
  g_sleepRequested = true;
}

static void cmdHeartbeat(const ParsedLine&, CmdSource, const CmdOut& out) {
  bool ok = lorawanSend(UplinkMsg::heartbeat(stateMailInside(), HeartbeatReason::Requested));
  reply(out, ok, ok ? "Heartbeat sent" : "Heartbeat failed");
  out.status(out.ctx, StatusComp::System);
}

static void cmdClear(const ParsedLine&, CmdSource src, const CmdOut& out) {
  stateSetMailInside(false);
  bool ok = lorawanSend(UplinkMsg::mailCleared(src == CmdSource::Downlink ? ClearSource::Downlink : ClearSource::Console));
  reply(out, ok, ok ? "Mail state cleared" : "Mail state cleared, uplink failed");
  out.status(out.ctx, StatusComp::System);
}

static void cmdStatus(const ParsedLine& args, CmdSource, const CmdOut& out) {
  static const char* const kNames[] = {"POWER", "IMU", "LORA", "KNOCK", "SYSTEM"};  // StatusComp 1..5
  const char* arg = args.argc ? args.args[0] : "ALL";
  if (strcasecmp(arg, "ALL") == 0) {
    for (uint8_t c = 1; c <= 5; c++) out.status(out.ctx, (StatusComp)c);
    return;
  }
  for (uint8_t c = 1; c <= 5; c++) {
    if (strcasecmp(arg, kNames[c - 1]) == 0) {
      out.status(out.ctx, (StatusComp)c);
      return;
    }
  }
  reply(out, false, "Unknown component. Use POWER|IMU|LORA|KNOCK|SYSTEM|ALL");
}

static void cmdStream(const ParsedLine& args, CmdSource src, const CmdOut& out) {
  if (src != CmdSource::Ble) {
    reply(out, false, "STREAM is only available over BLE");
    return;
  }
  uint16_t ms;
  if (!args.argc || !cmdParseStreamMs(args.args[0], BLE_STREAM_MIN_MS, BLE_STREAM_MAX_MS, ms)) {
    reply(out, false, "Usage: STREAM <ms>, 0 = off");
    return;
  }
  bleConsoleSetStreamMs(ms);
  char msg[CMD_REPLY_MAX + 1];
  if (ms) snprintf(msg, sizeof msg, "Stream every %u ms", ms);
  else snprintf(msg, sizeof msg, "Stream off");
  reply(out, true, msg);
  out.status(out.ctx, StatusComp::System);
}

static void cmdHelp(const ParsedLine&, CmdSource, const CmdOut& out);

static const CmdDef kCommands[] = {
    {"RECALIBRATE", 0, 0, cmdRecalibrate},
    {"TEST_UPLINK", 0, 0, cmdTestUplink},
    {"RESET_KNOCKS", 0, 0, cmdResetKnocks},
    {"DFU", 0, 0, cmdDfu},
    {"SLEEP", 0, 0, cmdSleep},
    {"HEARTBEAT", 0, 0x01, cmdHeartbeat},
    {"CLEAR", 0, 0x02, cmdClear},
    {"STATUS", 1, 0, cmdStatus},
    {"STREAM", 1, 0, cmdStream},
    {"HELP", 0, 0, cmdHelp},
};
static constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

// Replies are capped at CMD_REPLY_MAX bytes on the BLE wire, so the list is split across several lines.
static void cmdHelp(const ParsedLine&, CmdSource, const CmdOut& out) {
  char msg[CMD_REPLY_MAX + 1] = "Valid:";
  size_t n = strlen(msg);
  for (size_t i = 0; i < kCommandCount; i++) {
    bool last = i + 1 == kCommandCount;
    if (n + 1 + strlen(kCommands[i].name) + !last > CMD_REPLY_MAX) {
      reply(out, true, msg);
      n = 0;
      msg[0] = 0;
    }
    n += snprintf(msg + n, sizeof(msg) - n, "%s%s%s", n ? " " : "", kCommands[i].name, last ? "" : ",");
  }
  reply(out, true, msg);
}

// Handlers that transmit may have received further downlinks; process them (bounded recursion).
static void drainNested(const CmdOut& out) {
  if (g_depth >= MAX_DEPTH) return;
  g_depth++;
  cmdDrainDownlinks(out);
  g_depth--;
}

static bool dispatch(const CmdDef& def, const ParsedLine& args, CmdSource src, const CmdOut& out) {
  DBG_PRINTF("[CMD] %s\n", def.name);
  def.handler(args, src, out);
  drainNested(out);
  return true;
}

bool cmdRunLine(const char* line, CmdSource src, const CmdOut& out) {
  ParsedLine p;
  if (!cmdParseLine(line, p)) {
    reply(out, false, "Malformed command. Format: PIN:xxxxxx CMD:yyyy");
    return false;
  }
  if (src == CmdSource::Ble && (!p.hasPin || strcmp(p.pin, BLE_AUTH_PIN) != 0)) {
    DBG_PRINTLN("[BLE AUTH FAIL] Invalid PIN attempted");
    reply(out, false, "Authentication failed: Invalid PIN");
    return false;
  }
  for (size_t i = 0; i < kCommandCount; i++) {
    if (strcmp(p.name, kCommands[i].name) != 0) continue;
    if (p.argc > kCommands[i].maxArgs) {
      reply(out, false, "Too many arguments");
      return false;
    }
    return dispatch(kCommands[i], p, src, out);
  }
  reply(out, false, "Unknown command. Try HELP");
  return false;
}

bool cmdRunDownlink(const uint8_t* data, size_t len, const CmdOut& out) {
  if (!data || len == 0 || data[0] == 0) return false;
  for (size_t i = 0; i < kCommandCount; i++) {
    if (kCommands[i].downlinkOpcode != data[0]) continue;
    ParsedLine none;
    memset(&none, 0, sizeof none);  // opcodes carry no arguments yet
    return dispatch(kCommands[i], none, CmdSource::Downlink, out);
  }
  DBG_PRINTF("[CMD] Unknown downlink opcode 0x%02X\n", data[0]);
  return false;
}

void cmdDrainDownlinks(const CmdOut& out) {
  uint8_t buf[16];
  for (uint8_t n = 0; n < MAX_DRAIN_FRAMES; n++) {
    uint8_t fPort = 0;
    size_t len = 0;
    if (!lorawanTakeDownlink(fPort, buf, sizeof(buf), len)) return;
    if (fPort == DOWNLINK_FPORT) cmdRunDownlink(buf, len, out);
  }
}

bool cmdSleepRequested() { return g_sleepRequested; }
void cmdClearSleepRequest() { g_sleepRequested = false; }
