// Awake window: Web Bluetooth maintenance console + serial CLI, one timeout.
#include "ble/console.h"
#include "settings.h"
#include <Arduino.h>
#include <stdio.h>
#include "board.h"
#include "version.h"
#include "cmd/cmd.h"
#include "cmd/serial_cli.h"
#include "core/log.h"
#include "core/power.h"
#include "core/state.h"
#include "lora/lorawan.h"
#include "motion/imu.h"
#include "motion/knock.h"

#if ENABLE_BLE_MAINTENANCE
#include <bluefruit.h>

static constexpr size_t MAX_LINE = 64;  // BLE input is untrusted; bound the command buffer.

static BLEUart g_uart;
static uint32_t g_knockCount = 0;
static bool g_bleStarted = false;

static void bleWrite(void*, const char* line) { g_uart.println(line); }
static const CmdOut kBleOut = {bleWrite, nullptr};

void bleConsoleResetKnocks() { g_knockCount = 0; }
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
        bleWrite(nullptr, "{\"status\":\"ERROR\",\"msg\":\"Line too long\"}");
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

// Live knock counting plus the 1 Hz telemetry stream while a client is connected.
static void bleStream(bool connected, uint32_t now, uint32_t& lastKnock, uint32_t& lastTelemetry, uint32_t& lastAdvLog) {
#if ENABLE_BLE_MAINTENANCE
  AccelSample a = imuRead();

  if (knockHit(a.mag, now, lastKnock)) {
    g_knockCount++;
    DBG_PRINTF("[BLE KNOCK] #%lu detected! Mag: %.2fg\n", (unsigned long)g_knockCount, a.mag);
    if (connected) {
      char json[64];
      snprintf(json, sizeof(json), "{\"type\":\"knock\",\"count\":%lu,\"mag\":%.2f}", (unsigned long)g_knockCount, a.mag);
      g_uart.println(json);
    }
  }

  if (now - lastTelemetry <= 1000) return;
  lastTelemetry = now;
  int8_t temp = temperatureC();
  uint16_t vbat = batteryMv();
  uint32_t fcnt = lorawanFrameCounter();

  if (connected) {
    char json[448];
    snprintf(json, sizeof(json),
             "{\"type\":\"telemetry\",\"v_rest\":%u,\"v_sag\":%u,\"v_live\":%u,\"temp\":%d,\"tilt\":%.1f,"
             "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"mag\":%.2f,\"zero\":%.1f,\"fcnt\":%lu,"
             "\"lora_session\":\"%s\",\"last_payload\":\"%s\",\"knocks\":%lu,\"mail_inside\":%s,\"fw\":\"%s\"}",
             batteryRestMv(), batterySagMv(), vbat, temp, a.pitch, a.x, a.y, a.z, a.mag, imuTiltZero(),
             (unsigned long)fcnt, lorawanSessionStatus(), lorawanLastPayloadHex(), (unsigned long)g_knockCount,
             stateMailInside() ? "true" : "false", FIRMWARE_VERSION);
    g_uart.println(json);

    DBG_PRINTF("[BLE CONNECTED] V_live=%umV Pitch=%.1f° Accel=(%.2f,%.2f,%.2f) Mag=%.2fg Temp=%d°C Knocks=%lu FCnt=%lu\n",
               vbat, a.pitch, a.x, a.y, a.z, a.mag, temp, (unsigned long)g_knockCount, (unsigned long)fcnt);
  } else if (now - lastAdvLog > 4000) {
    lastAdvLog = now;
    DBG_PRINTF("[BLE ADVERTISING] Device: '%s' | V_live: %u mV | Temp: %d °C | IMU: %s (Connect via Web Console)\n",
               BLE_DEVICE_NAME, vbat, temp, imuPresent() ? "OK" : "NOT CONNECTED (plug into D4/D5)");
  }
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
  uint32_t lastAdvLog = 0;

  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);

  while (!cmdSleepRequested()) {
    watchdogFeed();
    serialCliPoll();
    blePollCommands();

    uint32_t now = millis();
    bool connected = bleConnected();
    digitalWrite(LED_BLUE, connected ? XIAO_LED_ON : XIAO_LED_OFF);  // Solid blue when connected

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

    bleStream(connected, now, lastKnock, lastTelemetry, lastAdvLog);
    delay(40);
  }

  if (cmdSleepRequested()) {
    DBG_PRINTLN("[AWAKE] Sleep requested. Closing window and entering deep sleep.");
  } else {
    DBG_PRINTLN("[AWAKE TIMEOUT] No client for the timeout period. Closing window and entering deep sleep.");
  }

  bleStop();
  pinMode(LED_BLUE, INPUT);
}
