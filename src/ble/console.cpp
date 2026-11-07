// Web Bluetooth maintenance console.
#include "ble/console.h"
#include "settings.h"

#if ENABLE_BLE_MAINTENANCE
#include <Arduino.h>
#include <bluefruit.h>
#include <stdio.h>
#include "board.h"
#include "version.h"
#include "core/log.h"
#include "core/power.h"
#include "lora/lorawan.h"
#include "motion/imu.h"
#include "motion/knock.h"

static constexpr size_t MAX_LINE = 64;  // BLE input is untrusted; bound the command buffer.

static BLEUart g_uart;
static uint32_t g_knockCount = 0;
static bool g_sleepRequested = false;
static bool g_bleStarted = false;

static bool startAdvertising() {
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
}

static void reply(bool ok, const char* msg) {
  char json[160];
  snprintf(json, sizeof(json), "{\"status\":\"%s\",\"msg\":\"%s\"}", ok ? "OK" : "ERROR", msg);
  g_uart.println(json);
}

static void handleCommand(const String& line) {
  const String pinPrefix = "PIN:";
  int pinIdx = line.indexOf(pinPrefix);
  int cmdIdx = line.indexOf("CMD:");

  if (pinIdx == -1 || cmdIdx == -1) {
    reply(false, "Malformed command. Format: PIN:xxxxxx CMD:yyyy");
    return;
  }

  String enteredPin = line.substring(pinIdx + pinPrefix.length(), line.indexOf(' ', pinIdx));
  enteredPin.trim();

  if (enteredPin != BLE_AUTH_PIN) {
    DBG_PRINTF("[BLE AUTH FAIL] Invalid PIN attempted: %s\n", enteredPin.c_str());
    reply(false, "Authentication failed: Invalid PIN");
    return;
  }

  String action = line.substring(cmdIdx + 4);
  action.trim();
  action.toUpperCase();

  DBG_PRINTF("[BLE COMMAND] Executing authorized action: %s\n", action.c_str());

  if (action == "RECALIBRATE") {
    float zero = imuCalibrateZero();
    char json[160];
    snprintf(json, sizeof(json), "{\"status\":\"OK\",\"msg\":\"Zero horizon calibrated\",\"zero_offset\":%.2f}", zero);
    g_uart.println(json);
    DBG_PRINTF("[CALIBRATION] New zero horizon offset: %.2f deg\n", zero);
  } else if (action == "TEST_UPLINK") {
    reply(true, "Triggering LoRaWAN test uplink...");
    // Byte 3 carries the zero offset so the console can show it.
    bool ok = lorawanSendEvent(UplinkEvent::ManualTest, imuRead().pitch, imuTiltZero(), 0);
    char json[200];
    snprintf(json, sizeof(json),
             "{\"status\":\"%s\",\"msg\":\"LoRaWAN test packet finished\",\"fcnt\":%lu,\"v_sag\":%u,\"payload\":\"%s\"}",
             ok ? "OK" : "ERROR", (unsigned long)lorawanFrameCounter(), batterySagMv(), lorawanLastPayloadHex());
    g_uart.println(json);
  } else if (action == "RESET_KNOCKS") {
    g_knockCount = 0;
    reply(true, "Knock counter reset to zero");
  } else if (action == "DFU") {
    reply(true, "Rebooting to UF2 Bootloader...");
    delay(300);
    enterUf2Dfu();
  } else if (action == "SLEEP") {
    reply(true, "Entering Deep Sleep...");
    delay(300);
    g_sleepRequested = true;
  } else {
    reply(false, "Unknown action. Valid: RECALIBRATE, TEST_UPLINK, RESET_KNOCKS, DFU, SLEEP");
  }
}

// Drains the UART into line; returns true if at least one command ran.
static bool pollCommands(String& line) {
  bool ran = false;
  while (g_uart.available()) {
    char c = (char)g_uart.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        handleCommand(line);
        line = "";
        ran = true;
      }
    } else {
      if (line.length() >= MAX_LINE) line = "";
      line += c;
    }
  }
  return ran;
}

void bleConsoleRun(uint32_t idleTimeoutS) {
  DBG_PRINTLN("\n>>> Starting WebBluetooth Maintenance Mode <<<");
  DBG_PRINTF("Device Name: %s | PIN: %s | Inactivity Timeout: %lu s\n", BLE_DEVICE_NAME, BLE_AUTH_PIN, (unsigned long)idleTimeoutS);
  DBG_PRINTLN("Mode: Stays awake indefinitely while client device is connected.");

  bool advOk = startAdvertising();
  DBG_PRINTF("[BLE] Radio Advertising Broadcast: %s\n", advOk ? "ACTIVE (Broadcasting)" : "FAILED (Data Size Error)");

  g_sleepRequested = false;
  uint32_t lastActivity = millis();
  uint32_t lastTelemetry = 0;
  uint32_t lastKnock = 0;
  uint32_t lastBlink = 0;
  uint32_t lastSerialAdvLog = 0;
  String line = "";

  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);

  while (!g_sleepRequested) {
    watchdogFeed();

    if (pollCommands(line)) lastActivity = millis();  // Valid interaction refreshes the timeout.

    bool connected = Bluefruit.connected();
    digitalWrite(LED_BLUE, connected ? XIAO_LED_ON : XIAO_LED_OFF);  // Solid blue when connected

    if (connected) {
      lastActivity = millis();
    } else if (benchMode()) {
      // Keep advertising indefinitely for bench testing.
      lastActivity = millis();
      // Heartbeat pulse: LED is dark and flashes ON for 40ms every 2.5s
      if (millis() - lastBlink > 2500) {
        lastBlink = millis();
        digitalWrite(LED_BLUE, XIAO_LED_ON);
        delay(40);
        digitalWrite(LED_BLUE, XIAO_LED_OFF);
      }
    } else if (millis() - lastActivity > idleTimeoutS * 1000UL) {
      break;
    }

    AccelSample a = imuRead();
    uint32_t now = millis();

    // Live knock detection while the console is up
    if (knockHit(a.mag, now, lastKnock)) {
      g_knockCount++;
      lastActivity = now;
      DBG_PRINTF("[BLE KNOCK] #%lu detected! Mag: %.2fg\n", (unsigned long)g_knockCount, a.mag);

      if (connected) {
        char json[64];
        snprintf(json, sizeof(json), "{\"type\":\"knock\",\"count\":%lu,\"mag\":%.2f}", (unsigned long)g_knockCount, a.mag);
        g_uart.println(json);
      }
    }

    // Telemetry stream every 1000 ms
    if (now - lastTelemetry > 1000) {
      lastTelemetry = now;
      int8_t temp = temperatureC();
      uint16_t vbat = batteryMv();
      uint32_t fcnt = lorawanFrameCounter();

      if (connected) {
        char json[384];
        snprintf(json, sizeof(json),
                 "{\"type\":\"telemetry\",\"v_rest\":%u,\"v_sag\":%u,\"v_live\":%u,\"temp\":%d,\"tilt\":%.1f,"
                 "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"mag\":%.2f,\"zero\":%.1f,\"fcnt\":%lu,"
                 "\"lora_session\":\"%s\",\"last_payload\":\"%s\",\"knocks\":%lu,\"fw\":\"%s\"}",
                 batteryRestMv(), batterySagMv(), vbat, temp, a.pitch, a.x, a.y, a.z, a.mag, imuTiltZero(),
                 (unsigned long)fcnt, lorawanSessionStatus(), lorawanLastPayloadHex(), (unsigned long)g_knockCount,
                 FIRMWARE_VERSION);
        g_uart.println(json);

        DBG_PRINTF("[BLE CONNECTED] V_live=%umV Pitch=%.1f° Accel=(%.2f,%.2f,%.2f) Mag=%.2fg Temp=%d°C Knocks=%lu FCnt=%lu\n",
                   vbat, a.pitch, a.x, a.y, a.z, a.mag, temp, (unsigned long)g_knockCount, (unsigned long)fcnt);
      } else if (now - lastSerialAdvLog > 4000) {
        lastSerialAdvLog = now;
        DBG_PRINTF("[BLE ADVERTISING] Device: '%s' | V_live: %u mV | Temp: %d °C | IMU: %s (Connect via Web Console)\n",
                   BLE_DEVICE_NAME, vbat, temp, imuPresent() ? "OK" : "NOT CONNECTED (plug into D4/D5)");
      }
    }

    delay(40);
  }

  if (g_sleepRequested) {
    DBG_PRINTLN("[BLE] Sleep requested. Terminating BLE and entering deep sleep.");
  } else {
    DBG_PRINTLN("[BLE TIMEOUT] Inactivity period reached. Terminating BLE and entering deep sleep.");
  }

  Bluefruit.disconnect(0);
  Bluefruit.Advertising.stop();
  pinMode(LED_BLUE, INPUT);
}

#endif
