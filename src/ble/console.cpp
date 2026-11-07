// Awake window: Web Bluetooth maintenance console + serial CLI, one timeout.
#include "ble/console.h"
#include "settings.h"
#include <Arduino.h>
#include "board.h"
#include "cmd/cmd.h"
#include "cmd/serial_cli.h"
#include "core/log.h"
#include "core/power.h"
#include "motion/imu.h"
#include "motion/knock.h"

#if ENABLE_BLE_MAINTENANCE
#include <bluefruit.h>
#include "ble/proto.h"

static constexpr size_t MAX_LINE = 64;  // BLE input is untrusted; bound the command buffer.

static BLEUart g_uart;
static uint32_t g_knockCount = 0;
static uint16_t g_streamMs = BLE_STREAM_DEFAULT_MS;  // 0 = off
static bool g_bleStarted = false;

void bleConsoleResetKnocks() { g_knockCount = 0; }
uint16_t bleConsoleKnockCount() { return (uint16_t)g_knockCount; }
uint16_t bleConsoleStreamMs() { return g_streamMs; }
void bleConsoleSetStreamMs(uint16_t ms) { g_streamMs = ms; }

static void bleSend(const uint8_t* buf, size_t len) {
  g_uart.write(buf, len);  // BLEUart splits frames larger than the MTU across notifications
}

static void bleReply(void*, bool ok, const char* msg) {
  uint8_t b[PROTO_MAX_FRAME];
  bleSend(b, protoReply(b, ok, msg));
}

static size_t encodeStatus(uint8_t* b, StatusComp comp, const StatusData& d) {
  switch (comp) {
    case StatusComp::Power:
      return protoStatusPower(b, d.restMv, d.sagMv, d.liveMv, d.tempC);
    case StatusComp::Imu:
      return protoStatusImu(b, d.imuPresent, protoI16(d.pitch * 10.0f), protoI16(d.ax * 1000.0f),
                            protoI16(d.ay * 1000.0f), protoI16(d.az * 1000.0f), protoU16(d.mag * 1000.0f),
                            protoI16(d.zero * 10.0f));
    case StatusComp::Lora:
      return protoStatusLora(b, d.fcnt, d.loraSession, d.lastPayload);
    case StatusComp::Knock:
      return protoStatusKnock(b, d.knocks);
    case StatusComp::System:
      return protoStatusSystem(b, d.bench, d.usb, d.mailInside, d.streamMs, d.fw);
  }
  return 0;
}

static void bleStatus(void*, StatusComp comp) {
  StatusData d;
  statusRead(d);
  uint8_t b[PROTO_MAX_FRAME];
  size_t n = encodeStatus(b, comp, d);
  if (n) bleSend(b, n);
}

static const CmdOut kBleOut = {bleReply, bleStatus, nullptr};
#endif

static bool bleStart() {
#if ENABLE_BLE_MAINTENANCE
  if (!g_bleStarted) {
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    Bluefruit.begin();
    Bluefruit.setTxPower(4);
    Bluefruit.setName(BLE_DEVICE_NAME);

    g_uart.begin();

    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addService(g_uart);
    Bluefruit.Advertising.addName();
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    g_bleStarted = true;
  }
  return Bluefruit.Advertising.start(0);  // 0 = continuous radio broadcast
#else
  return false;
#endif
}

static bool bleConnected() {
#if ENABLE_BLE_MAINTENANCE
  return Bluefruit.connected();
#else
  return false;
#endif
}

static void bleStop() {
#if ENABLE_BLE_MAINTENANCE
  Bluefruit.disconnect(0);
  Bluefruit.Advertising.stop();
#endif
}

// Drains the UART; every complete line goes to the command interpreter.
static void blePollCommands() {
#if ENABLE_BLE_MAINTENANCE
  static char line[MAX_LINE + 1];
  static size_t len = 0;
  static bool overflow = false;
  while (g_uart.available()) {
    char c = (char)g_uart.read();
    if (c == '\n' || c == '\r') {
      if (overflow) {
        bleReply(nullptr, false, "Line too long");
      } else if (len > 0) {
        line[len] = 0;
        cmdRunLine(line, CmdSource::Ble, kBleOut);
      }
      len = 0;
      overflow = false;
    } else if (len < MAX_LINE) {
      line[len++] = c;
    } else {
      overflow = true;
    }
  }
#endif
}

// Live knock counting, plus binary knock and telemetry frames while a client is connected.
static void bleStream(bool connected, uint32_t now, uint32_t& lastKnock, uint32_t& lastTelemetry) {
#if ENABLE_BLE_MAINTENANCE
  AccelSample a = imuRead();

  if (knockHit(a.mag, now, lastKnock)) {
    g_knockCount++;
    DBG_PRINTF("[BLE KNOCK] #%lu detected! Mag: %.2fg\n", (unsigned long)g_knockCount, a.mag);
    if (connected) {
      uint8_t b[PROTO_MAX_FRAME];
      bleSend(b, protoKnock(b, (uint16_t)g_knockCount, protoU16(a.mag * 1000.0f)));
    }
  }

  if (!connected || g_streamMs == 0 || now - lastTelemetry < g_streamMs) return;
  lastTelemetry = now;
  StatusData d;
  statusRead(d);
  uint8_t b[PROTO_MAX_FRAME];
  bleSend(b, protoTelemetry(b, d.liveMv, d.tempC, protoI16(d.pitch * 10.0f), protoI16(d.ax * 1000.0f),
                            protoI16(d.ay * 1000.0f), protoI16(d.az * 1000.0f), protoU16(d.mag * 1000.0f),
                            d.knocks, d.mailInside));
#endif
}

void bleConsoleRun() {
  DBG_PRINTLN("\n>>> Starting awake window <<<");
  DBG_PRINTF("Device Name: %s | PIN: %s | Timeout: %u s\n", BLE_DEVICE_NAME, BLE_AUTH_PIN, (unsigned)BLE_TIMEOUT_SECONDS);
  DBG_PRINTLN("Mode: Stays awake while a client is connected.");

  bool advOk = bleStart();
  DBG_PRINTF("[BLE] Radio Advertising Broadcast: %s\n", advOk ? "ACTIVE (Broadcasting)" : "OFF/FAILED");
  serialCliBegin();

  cmdClearSleepRequest();
  uint32_t lastActive = millis();  // window opened, or a client was last connected
  uint32_t lastTelemetry = 0;
  uint32_t lastKnock = 0;
  uint32_t lastBlink = 0;
  bool wasConnected = false;

  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);

  while (!cmdSleepRequested()) {
    watchdogFeed();
    serialCliPoll();
    blePollCommands();

    uint32_t now = millis();
    bool connected = bleConnected();
    digitalWrite(LED_BLUE, connected ? XIAO_LED_ON : XIAO_LED_OFF);  // Solid blue when connected

    if (connected != wasConnected) {
      wasConnected = connected;
      if (connected) bleConsoleSetStreamMs(BLE_STREAM_DEFAULT_MS);
      DBG_PRINTLN(connected ? "[BLE] Client connected" : "[BLE] Client disconnected");
    }

    if (connected) {
      lastActive = now;
    } else if (benchMode()) {
      // Keep advertising indefinitely for bench testing.
      lastActive = now;
      // Heartbeat pulse: LED is dark and flashes ON for 40ms every 2.5s
      if (now - lastBlink > 2500) {
        lastBlink = now;
        digitalWrite(LED_BLUE, XIAO_LED_ON);
        delay(40);
        digitalWrite(LED_BLUE, XIAO_LED_OFF);
      }
    } else if (now - lastActive > BLE_TIMEOUT_SECONDS * 1000UL) {
      break;
    }

    bleStream(connected, now, lastKnock, lastTelemetry);
    delay(20);  // loop period must stay under BLE_STREAM_MIN_MS
  }

  if (cmdSleepRequested()) {
    DBG_PRINTLN("[AWAKE] Sleep requested. Closing window and entering deep sleep.");
  } else {
    DBG_PRINTLN("[AWAKE TIMEOUT] No client for the timeout period. Closing window and entering deep sleep.");
  }

  bleStop();
  pinMode(LED_BLUE, INPUT);
}
