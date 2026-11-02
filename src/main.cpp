#include <Arduino.h>
#include <Wire.h>
#include <RadioLib.h>
#include <bluefruit.h>
#include "settings.h"
#include "config.h"

#define FIRMWARE_VERSION "1.2.0"

#define EVENT_MAIL_DELIVERED 0x01
#define EVENT_MAILBOX_EMPTIED 0x02
#define EVENT_MANUAL_TEST     0x03

#define XIAO_LED_ON           LOW
#define XIAO_LED_OFF          HIGH

#if DEBUG_SERIAL
  #define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

BLEUart bleuart;

SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);
LoRaWANNode node(&radio, &LORAWAN_BAND);

static float g_tiltZeroOffset = 0.0f;
static uint16_t g_lastVbatRest = 3000;
static uint16_t g_lastVbatSag = 0;
static char g_lastPayloadHex[48] = "None";
static const char* g_loraSessionStatus = "COLD_BOOT";
static uint32_t g_liveKnockCount = 0;
static bool g_imuDetected = false;

static inline bool isUsbConnected() {
  return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}
#if ENABLE_SESSION_PERSISTENCE
#define SESSION_MAGIC 0xA55A1234
__attribute__((section(".noinit"))) static uint32_t g_sessionMagic;
__attribute__((section(".noinit"))) static uint8_t g_savedNonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
__attribute__((section(".noinit"))) static uint8_t g_savedSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
#endif

#if ENABLE_WATCHDOG
static void initWatchdog(uint32_t timeoutSec) {
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Pause << WDT_CONFIG_SLEEP_Pos);
  NRF_WDT->CRV = (timeoutSec * 32768) - 1;
  NRF_WDT->RREN = (WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos);
  NRF_WDT->TASKS_START = 1;
}

static inline void feedWatchdog() {
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}
#endif

struct AccelData {
  float x;      // in g
  float y;      // in g
  float z;      // in g
  float pitch;  // calibrated pitch angle in degrees
  float mag;    // vector magnitude in g
};

static void enterDeepSleep();
static void startBleMaintenance(uint32_t advTimeoutSeconds);
static uint64_t parseHex64(const char* str) {
  uint64_t val = 0;
  while (*str) {
    char c = *str++;
    if (c >= '0' && c <= '9')      val = (val << 4) | (c - '0');
    else if (c >= 'a' && c <= 'f') val = (val << 4) | (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') val = (val << 4) | (c - 'A' + 10);
  }
  return val;
}

static void parseHexBytes(const char* str, uint8_t* out, size_t maxBytes) {
  size_t count = 0;
  uint8_t currentByte = 0;
  bool highNibble = true;

  while (*str && count < maxBytes) {
    char c = *str++;
    int8_t nibble = -1;
    if (c >= '0' && c <= '9')      nibble = c - '0';
    else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;

    if (nibble != -1) {
      if (highNibble) {
        currentByte = (uint8_t)(nibble << 4);
        highNibble = false;
      } else {
        currentByte |= (uint8_t)nibble;
        out[count++] = currentByte;
        highNibble = true;
      }
    }
  }
}

static void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)1);
  return Wire.read();
}

void setupLIS3DH() {
  Wire.begin();
  delay(15);

  uint8_t id = readReg(0x0F);
  if (id != 0x33) {
    g_imuDetected = false;
#if DEBUG_SERIAL
    DBG_PRINTF("[IMU] LIS3DH not detected at 0x%02X (WHO_AM_I = 0x%02X, expected 0x33).\n", LIS3DH_I2C_ADDR, id);
    DBG_PRINTLN("[IMU] Board is in bench mode without IMU. Plug LIS3DH to D4/D5 when ready.");
#endif
    return;
  }

  g_imuDetected = true;
#if DEBUG_SERIAL
  DBG_PRINTLN("[IMU] LIS3DH detected (WHO_AM_I = 0x33). Initializing registers...");
#endif

  // Set 1 Hz (~2 µA) or 10 Hz (~6 µA) low-power sample rate.
#if SENSOR_1HZ_ULTRA_LOW_POWER
  writeReg(0x20, 0x1F);
#else
  writeReg(0x20, 0x2F);
#endif

  // High-pass filter in reference mode detects motion deltas rather than static tilt.
  writeReg(0x21, 0x09);
  writeReg(0x22, 0x40); // Route interrupt 1 to INT1 pin.
  writeReg(0x23, 0x00); // +/-2g full scale.
  writeReg(0x32, 0x0A); // Motion threshold: ~160 mg (~10-12 deg deflection).
  writeReg(0x33, 0x01); // 1 sample duration filter.
  writeReg(0x30, 0x2A); // Enable X/Y/Z high-event interrupt generation.

  // Latch current resting orientation into the reference register.
  readReg(0x26);
}

AccelData readAccel() {
  AccelData a = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  if (!g_imuDetected) {
    return a;
  }
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(0x28 | 0x80); // Auto-increment from OUT_X_L
  if (Wire.endTransmission(false) != 0) {
    return a;
  }
  if (Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)6) < 6) {
    return a;
  }

  int16_t rawX = Wire.read() | (Wire.read() << 8);
  int16_t rawY = Wire.read() | (Wire.read() << 8);
  int16_t rawZ = Wire.read() | (Wire.read() << 8);

  a.x = (float)rawX / 16384.0f;
  a.y = (float)rawY / 16384.0f;
  a.z = (float)rawZ / 16384.0f;
  a.mag = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);

  float pitch = atan2(a.x, sqrt(a.y * a.y + a.z * a.z)) * 180.0f / PI;
  pitch = fabs(pitch) - g_tiltZeroOffset;
  a.pitch = (pitch < 0.0f) ? 0.0f : pitch;
  return a;
}

float getPitchAngle() {
  return readAccel().pitch;
}

float getAccelerationMagnitude() {
  return readAccel().mag;
}

uint16_t getBatteryMilliVolts() {
  analogReference(AR_INTERNAL); // 0.6V bandgap reference
  analogReadResolution(12);     // 12-bit ADC

#if defined(PIN_VBAT_ENABLE) && defined(PIN_VBAT)
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW); // Enable divider
  delay(1);
  uint32_t raw = analogRead(PIN_VBAT);
  digitalWrite(PIN_VBAT_ENABLE, HIGH); // Disable divider immediately to eliminate parasitic current
  // XIAO nRF52840 divider ratio: 1M / 510k (factor = 1.51) -> raw * (3.6 / 4096) * 1.51
  uint16_t mv = (uint16_t)(raw * 3600 / 4096 * 1.51);
  return mv;
#else
  return 3000;
#endif
}

int8_t getTemperatureC() {
#if ENABLE_TEMPERATURE
  return (int8_t)round(readCPUTemperature());
#else
  return 20;
#endif
}

static void enterDeepSleep() {
  // If USB is connected or IMU is absent, do NOT enter un-wakeable systemOff!
  if (isUsbConnected() || !g_imuDetected) {
#if DEBUG_SERIAL
    DBG_PRINTLN("\n[BENCH MODE] Device will not deep-sleep: USB is plugged in or IMU is absent.");
    DBG_PRINTLN("[BENCH MODE] Continuing WebBluetooth maintenance mode...");
#endif
    startBleMaintenance(BLE_TIMEOUT_SECONDS);
    return;
  }

#if DEBUG_SERIAL
  DBG_PRINTLN("[POWER] Entering System-Off Deep Sleep (wake on INT1 pin)...");
  Serial.flush();
#endif

  radio.sleep();
  readReg(0x31);
  readReg(0x26); // Latch resting vector to prevent wake loops if flap remains propped open.

  // Tri-state LEDs to prevent active-low parasitic draw.
  pinMode(LED_RED, INPUT);
  pinMode(LED_GREEN, INPUT);
  pinMode(LED_BLUE, INPUT);

  systemOff(PIN_ACCEL_INT, 1);
}

static bool sendLoRaWANPacket(uint8_t* payload, size_t len, uint8_t fPort) {
#if ENABLE_WATCHDOG
  feedWatchdog();
#endif

  // Cache hex string for telemetry
  char* p = g_lastPayloadHex;
  for (size_t i = 0; i < len && i < 12; i++) {
    p += sprintf(p, "%02X ", payload[i]);
  }
  if (p > g_lastPayloadHex) *(p - 1) = '\0';

#if DEBUG_SERIAL
  DBG_PRINTLN("\n----------------------------------------------");
  DBG_PRINTLN("[LoRaWAN TX] Preparing Uplink Transmission");
  DBG_PRINTF("[LoRaWAN TX] JoinEUI: %s | DevEUI: %s\n", TTN_JOIN_EUI_HEX, TTN_DEV_EUI_HEX);
  DBG_PRINTF("[LoRaWAN TX] Retained RAM Session: %s\n",
             (g_sessionMagic == SESSION_MAGIC) ? "VALID (Restoring Session)" : "COLD_BOOT (Full OTAA Join Needed)");
#endif

  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
#if DEBUG_SERIAL
    DBG_PRINTF("[LoRaWAN TX ERROR] Radio begin failed: %d\n", state);
#endif
    return false;
  }

  radio.setRegulatorDCDC(); // Switch module from LDO to internal buck regulator.

#if ENABLE_ADR
  node.setADR(true);
#endif

  uint64_t joinEUI = parseHex64(TTN_JOIN_EUI_HEX);
  uint64_t devEUI  = parseHex64(TTN_DEV_EUI_HEX);
  uint8_t appKey[16];
  uint8_t nwkKey[16];
  parseHexBytes(TTN_APP_KEY_HEX, appKey, 16);

#ifdef TTN_NWK_KEY_HEX
  parseHexBytes(TTN_NWK_KEY_HEX, nwkKey, 16);
#else
  memcpy(nwkKey, appKey, 16);
#endif

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

#if ENABLE_SESSION_PERSISTENCE
  if (g_sessionMagic == SESSION_MAGIC) {
    node.setBufferNonces(g_savedNonces);
    node.setBufferSession(g_savedSession);
  }
#endif

  state = node.activateOTAA();
  if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    g_loraSessionStatus = "RESTORED";
  } else if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    g_loraSessionStatus = "NEW_JOINED";
  } else if (state == RADIOLIB_ERR_NONE) {
    g_loraSessionStatus = "ACTIVE";
  } else {
    g_loraSessionStatus = "FAILED";
#if DEBUG_SERIAL
    DBG_PRINTF("[LoRaWAN TX ERROR] Activation failed with code %d\n", state);
#endif
    return false;
  }

#if ENABLE_SESSION_PERSISTENCE
  if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    memcpy(g_savedNonces, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    memcpy(g_savedSession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    g_sessionMagic = SESSION_MAGIC;
  }
#endif

#if ENABLE_WATCHDOG
  feedWatchdog();
#endif

#if DEBUG_SERIAL
  DBG_PRINTF("[LoRaWAN TX] Activation State: %s (code %d)\n", g_loraSessionStatus, state);
  DBG_PRINTF("[LoRaWAN TX] FCntUp: %lu | FPort: %u | Length: %u bytes\n", node.getFCntUp(), fPort, len);
  DBG_PRINTF("[LoRaWAN TX] Hex Payload: [%s]\n", g_lastPayloadHex);
  DBG_PRINTLN("[LoRaWAN TX] Transmitting packet to Gateway...");
#endif

  int16_t sendState = node.sendReceive(payload, len, fPort);

#if ENABLE_SESSION_PERSISTENCE
  memcpy(g_savedSession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
#endif

#if ENABLE_BATTERY_SAG
  uint16_t vbatLoad = getBatteryMilliVolts();
  g_lastVbatSag = (g_lastVbatRest > vbatLoad) ? (g_lastVbatRest - vbatLoad) : 0;
#endif

#if DEBUG_SERIAL
  DBG_PRINTF("[LoRaWAN TX] Result: %s (code %d)\n", (sendState == RADIOLIB_ERR_NONE) ? "SUCCESS" : "ERROR", sendState);
  DBG_PRINTF("[LoRaWAN TX] Battery: Rest=%u mV | Load=%u mV | Sag=%u mV\n", g_lastVbatRest, vbatLoad, g_lastVbatSag);
  DBG_PRINTLN("----------------------------------------------\n");
#endif

#if ENABLE_WATCHDOG
  feedWatchdog();
#endif

  return (sendState == RADIOLIB_ERR_NONE);
}

#if ENABLE_BLE_MAINTENANCE
static void processBleCommand(const String& cmd) {
  String pinPrefix = "PIN:";
  int pinIdx = cmd.indexOf(pinPrefix);
  int cmdIdx = cmd.indexOf("CMD:");

  if (pinIdx == -1 || cmdIdx == -1) {
    bleuart.println("{\"status\":\"ERROR\",\"msg\":\"Malformed command. Format: PIN:xxxxxx CMD:yyyy\"}");
    return;
  }

  String enteredPin = cmd.substring(pinIdx + pinPrefix.length(), cmd.indexOf(' ', pinIdx));
  enteredPin.trim();

  if (enteredPin != BLE_AUTH_PIN) {
#if DEBUG_SERIAL
    DBG_PRINTF("[BLE AUTH FAIL] Invalid PIN attempted: %s\n", enteredPin.c_str());
#endif
    bleuart.println("{\"status\":\"ERROR\",\"msg\":\"Authentication failed: Invalid PIN\"}");
    return;
  }

  String action = cmd.substring(cmdIdx + 4);
  action.trim();
  action.toUpperCase();

#if DEBUG_SERIAL
  DBG_PRINTF("[BLE COMMAND] Executing authorized action: %s\n", action.c_str());
#endif

  if (action == "RECALIBRATE") {
    float rawPitch = getPitchAngle() + g_tiltZeroOffset;
    g_tiltZeroOffset = rawPitch;
    bleuart.print("{\"status\":\"OK\",\"msg\":\"Zero horizon calibrated\",\"zero_offset\":");
    bleuart.print(g_tiltZeroOffset, 2);
    bleuart.println("}");
#if DEBUG_SERIAL
    DBG_PRINTF("[CALIBRATION] New zero horizon offset: %.2f deg\n", g_tiltZeroOffset);
#endif
  } else if (action == "TEST_UPLINK") {
    bleuart.println("{\"status\":\"OK\",\"msg\":\"Triggering LoRaWAN test uplink...\"}");
    uint8_t payload[7];
    payload[0] = (uint8_t)((g_lastVbatRest < 2000 ? 0 : (g_lastVbatRest > 4550 ? 255 : (g_lastVbatRest - 2000) / 10)));
    payload[1] = (uint8_t)(g_lastVbatSag / 5 > 255 ? 255 : g_lastVbatSag / 5);
    payload[2] = (uint8_t)round(getPitchAngle());
    payload[3] = (uint8_t)round(g_tiltZeroOffset);
    payload[4] = 0;
    payload[5] = (uint8_t)getTemperatureC();
    payload[6] = EVENT_MANUAL_TEST;

    bool ok = sendLoRaWANPacket(payload, sizeof(payload), 1);
    bleuart.print("{\"status\":");
    bleuart.print(ok ? "\"OK\"" : "\"ERROR\"");
    bleuart.print(",\"msg\":\"LoRaWAN test packet finished\",\"fcnt\":");
    bleuart.print(node.getFCntUp());
    bleuart.print(",\"v_sag\":");
    bleuart.print(g_lastVbatSag);
    bleuart.print(",\"payload\":\"");
    bleuart.print(g_lastPayloadHex);
    bleuart.println("\"}");
  } else if (action == "RESET_KNOCKS") {
    g_liveKnockCount = 0;
    bleuart.println("{\"status\":\"OK\",\"msg\":\"Knock counter reset to zero\"}");
  } else if (action == "DFU") {
    bleuart.println("{\"status\":\"OK\",\"msg\":\"Rebooting to UF2 Bootloader...\"}");
    delay(300);
    enterUf2Dfu();
  } else if (action == "SLEEP") {
    bleuart.println("{\"status\":\"OK\",\"msg\":\"Entering Deep Sleep...\"}");
    delay(300);
    Bluefruit.disconnect(0);
    Bluefruit.Advertising.stop();
    enterDeepSleep();
  } else {
    bleuart.println("{\"status\":\"ERROR\",\"msg\":\"Unknown action. Valid: RECALIBRATE, TEST_UPLINK, RESET_KNOCKS, DFU, SLEEP\"}");
  }
}

static void startBleMaintenance(uint32_t advTimeoutSeconds) {
#if DEBUG_SERIAL
  DBG_PRINTLN("\n>>> Starting WebBluetooth Maintenance Mode <<<");
  DBG_PRINTF("Device Name: %s | PIN: %s | Inactivity Timeout: %lu s\n", BLE_DEVICE_NAME, BLE_AUTH_PIN, advTimeoutSeconds);
  DBG_PRINTLN("Mode: Stays awake indefinitely while client device is connected.");
#endif

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(BLE_DEVICE_NAME);

  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(advTimeoutSeconds);
  Bluefruit.Advertising.start(advTimeoutSeconds);

  uint32_t lastActivityTime = millis();
  uint32_t lastTelemetry = 0;
  uint32_t lastBleKnockTime = 0;
  uint32_t lastBlinkTime = 0;
  uint32_t lastSerialAdvLog = 0;
  String rxBuffer = "";

  // Ensure LED starts OFF
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);

  while (true) {
#if ENABLE_WATCHDOG
    feedWatchdog();
#endif

    while (bleuart.available()) {
      char c = (char)bleuart.read();
      if (c == '\n' || c == '\r') {
        if (rxBuffer.length() > 0) {
          processBleCommand(rxBuffer);
          rxBuffer = "";
          lastActivityTime = millis(); // Refresh timeout on valid interaction
        }
      } else {
        rxBuffer += c;
      }
    }

    bool isConnected = Bluefruit.connected();

    if (isConnected) {
      lastActivityTime = millis();
      pinMode(LED_BLUE, OUTPUT);
      digitalWrite(LED_BLUE, XIAO_LED_ON); // Solid blue when connected
    } else {
      // If USB is connected or IMU is absent, keep advertising indefinitely for bench testing
      if (isUsbConnected() || !g_imuDetected) {
        lastActivityTime = millis();
        // Heartbeat pulse: LED is dark and flashes ON for 40ms every 2.5s
        if (millis() - lastBlinkTime > 2500) {
          lastBlinkTime = millis();
          digitalWrite(LED_BLUE, XIAO_LED_ON);
          delay(40);
          digitalWrite(LED_BLUE, XIAO_LED_OFF);
        }
      } else {
        if (millis() - lastActivityTime > ((uint32_t)advTimeoutSeconds * 1000UL)) {
          break;
        }
      }
    }

    AccelData a = readAccel();
    uint32_t now = millis();

    // Live knock detection while connected over WebBluetooth
    if (a.mag > 1.4f && (now - lastBleKnockTime > 180)) {
      lastBleKnockTime = now;
      g_liveKnockCount++;
      lastActivityTime = now;

#if DEBUG_SERIAL
      DBG_PRINTF("[BLE KNOCK] #%lu detected! Mag: %.2fg\n", g_liveKnockCount, a.mag);
#endif

      if (isConnected) {
        bleuart.print("{\"type\":\"knock\",\"count\":");
        bleuart.print(g_liveKnockCount);
        bleuart.print(",\"mag\":");
        bleuart.print(a.mag, 2);
        bleuart.println("}");
      }
    }

    // Telemetry stream every 1000 ms
    if (now - lastTelemetry > 1000) {
      lastTelemetry = now;
      int8_t temp = getTemperatureC();
      uint16_t vbat = getBatteryMilliVolts();
      uint32_t fcnt = node.getFCntUp();

      if (isConnected) {
        bleuart.print("{\"type\":\"telemetry\"");
        bleuart.print(",\"v_rest\":"); bleuart.print(g_lastVbatRest);
        bleuart.print(",\"v_sag\":"); bleuart.print(g_lastVbatSag);
        bleuart.print(",\"v_live\":"); bleuart.print(vbat);
        bleuart.print(",\"temp\":"); bleuart.print(temp);
        bleuart.print(",\"tilt\":"); bleuart.print(a.pitch, 1);
        bleuart.print(",\"ax\":"); bleuart.print(a.x, 2);
        bleuart.print(",\"ay\":"); bleuart.print(a.y, 2);
        bleuart.print(",\"az\":"); bleuart.print(a.z, 2);
        bleuart.print(",\"mag\":"); bleuart.print(a.mag, 2);
        bleuart.print(",\"zero\":"); bleuart.print(g_tiltZeroOffset, 1);
        bleuart.print(",\"fcnt\":"); bleuart.print(fcnt);
        bleuart.print(",\"lora_session\":\""); bleuart.print(g_loraSessionStatus); bleuart.print("\"");
        bleuart.print(",\"last_payload\":\""); bleuart.print(g_lastPayloadHex); bleuart.print("\"");
        bleuart.print(",\"knocks\":"); bleuart.print(g_liveKnockCount);
        bleuart.print(",\"fw\":\""); bleuart.print(FIRMWARE_VERSION); bleuart.print("\"");
        bleuart.println("}");
      }

#if DEBUG_SERIAL
      if (isConnected) {
        DBG_PRINTF("[BLE CONNECTED] V_live=%umV Pitch=%.1f° Accel=(%.2f,%.2f,%.2f) Mag=%.2fg Temp=%d°C Knocks=%lu FCnt=%lu\n",
                   vbat, a.pitch, a.x, a.y, a.z, a.mag, temp, g_liveKnockCount, fcnt);
      } else if (now - lastSerialAdvLog > 4000) {
        lastSerialAdvLog = now;
        DBG_PRINTF("[BLE ADVERTISING] Device: '%s' | V_live: %u mV | Temp: %d °C | IMU: %s (Connect via Web Console)\n",
                   BLE_DEVICE_NAME, vbat, temp, g_imuDetected ? "OK" : "NOT CONNECTED (plug into D4/D5)");
      }
#endif
    }

    delay(40);
  }

#if DEBUG_SERIAL
  DBG_PRINTLN("[BLE TIMEOUT] Inactivity period reached. Terminating BLE and entering deep sleep.");
#endif

  Bluefruit.disconnect(0);
  Bluefruit.Advertising.stop();
  pinMode(LED_BLUE, INPUT);
  enterDeepSleep();
}
#endif

#if ENABLE_KNOCK_DETECTION
static bool checkForKnockPattern() {
  uint32_t windowStart = millis();
  uint8_t knockCount = 1;
  uint32_t lastKnockTime = windowStart;

#if DEBUG_SERIAL
  DBG_PRINTF("[KNOCK] Initial vibration trigger! Starting %u ms pattern detection window...\n", KNOCK_WINDOW_MS);
  DBG_PRINTF("[KNOCK] Hit #1 detected | Window Start: %lu ms\n", windowStart);
#endif

  while (millis() - windowStart < KNOCK_WINDOW_MS) {
    if (knockCount == 1 && (millis() - windowStart > KNOCK_ABORT_TIMEOUT_MS)) {
#if DEBUG_SERIAL
      DBG_PRINTF("[KNOCK ABORT] Single transient vibration timed out after %u ms. Aborting.\n", KNOCK_ABORT_TIMEOUT_MS);
#endif
      return false; // Abort early on single transient vibrations.
    }

    AccelData a = readAccel();
    uint32_t now = millis();

    if (a.mag > 1.4f && (now - lastKnockTime > 160)) {
      knockCount++;
      lastKnockTime = now;
#if DEBUG_SERIAL
      DBG_PRINTF("[KNOCK] Hit #%u detected! Peak Mag: %.2fg | Delta: %lu ms\n", knockCount, a.mag, now - lastKnockTime);
#endif
      if (knockCount >= KNOCK_COUNT_REQUIRED) {
#if DEBUG_SERIAL
        DBG_PRINTF("[KNOCK PATTERN MATCHED] %u knocks confirmed! Mailbox Emptied event triggered.\n", KNOCK_COUNT_REQUIRED);
#endif
        return true;
      }
    }
    delay(20);
  }

#if DEBUG_SERIAL
  DBG_PRINTF("[KNOCK TIMEOUT] Window expired with only %u of %u knocks.\n", knockCount, KNOCK_COUNT_REQUIRED);
#endif
  return false;
}
#endif

void setup() {
#if ENABLE_WATCHDOG
  initWatchdog(WATCHDOG_TIMEOUT_SECONDS);
  feedWatchdog();
#endif

  // Blink Blue LED on power-up so the user immediately sees the board is running!
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_ON);
  delay(100);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);
  delay(80);
  digitalWrite(LED_BLUE, XIAO_LED_ON);
  delay(100);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);

#if DEBUG_SERIAL
  Serial.begin(115200);
  // Wait up to 2.5s for Serial Monitor if USB is plugged in
  uint32_t t0 = millis();
  while (!Serial && isUsbConnected() && (millis() - t0 < 2500)) {
    delay(10);
  }
  DBG_PRINTLN("\n==================================================");
  DBG_PRINTF("   MAILBOX TILT SENSOR [FW v%s]\n", FIRMWARE_VERSION);
  DBG_PRINTLN("   nRF52840 + SX1262 (EU868) + LIS3DH 3-Axis Accel");
  DBG_PRINTF("   Power: %s\n", isUsbConnected() ? "USB 5V (Bench Test Mode)" : "CR123A Battery 3V");
  DBG_PRINTLN("==================================================");
#endif

  uint32_t resetReason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = 0xFFFFFFFF; // Clear reset flags
  bool isWakeFromSleep = (resetReason & POWER_RESETREAS_OFF_Msk) != 0;

#if DEBUG_SERIAL
  DBG_PRINTF("[BOOT] Reset Reason: 0x%08X (WakeFromSleep=%s)\n", resetReason, isWakeFromSleep ? "YES" : "NO");
#endif

  NRF_POWER->DCDCEN = 1; // Enable DC-DC regulator for lower CPU active draw.

  pinMode(PIN_ACCEL_INT, INPUT);
  setupLIS3DH();

  g_lastVbatRest = getBatteryMilliVolts();
  AccelData initial = readAccel();

#if DEBUG_SERIAL
  DBG_PRINTF("[INIT] Battery Rest Voltage: %u mV\n", g_lastVbatRest);
  DBG_PRINTF("[INIT] Internal Temp:        %d °C\n", getTemperatureC());
  DBG_PRINTF("[INIT] LIS3DH Status:        %s\n", g_imuDetected ? "CONNECTED (OK)" : "DISCONNECTED (Bench Mode)");
  if (g_imuDetected) {
    DBG_PRINTF("[INIT] LIS3DH Accel:         X=%.2fg, Y=%.2fg, Z=%.2fg (Mag=%.2fg)\n",
               initial.x, initial.y, initial.z, initial.mag);
    DBG_PRINTF("[INIT] Initial Pitch Angle:  %.1f deg (Zero offset: %.1f deg)\n", initial.pitch, g_tiltZeroOffset);
  }
#endif

#if ENABLE_BLE_MAINTENANCE
  // If USB is plugged in, or IMU is absent, or on cold boot/power-up:
  if (!isWakeFromSleep || isUsbConnected() || !g_imuDetected) {
#if DEBUG_SERIAL
    DBG_PRINTLN("[BOOT] Advertising WebBluetooth as 'Mailbox-Sensor'...");
#endif
    startBleMaintenance(BLE_BOOT_ADVERTISE_SECONDS);
  }
#endif

  // If IMU is missing, enter safe idle rather than waking on noise
  if (!g_imuDetected) {
#if DEBUG_SERIAL
    DBG_PRINTLN("[HALT] IMU not detected. Waiting in bench mode...");
#endif
    enterDeepSleep();
  }
#if ENABLE_KNOCK_DETECTION
  // When an interrupt fires without tilt, check if it matches a knock pattern.
  if (initial.pitch < 8.0f) {
#if DEBUG_SERIAL
    DBG_PRINTLN("[TRIGGER] Accel INT1 fired while flap is closed (<8 deg). Checking knock pattern...");
#endif
    if (checkForKnockPattern()) {
      uint8_t payload[7];
      payload[0] = (uint8_t)((g_lastVbatRest < 2000 ? 0 : (g_lastVbatRest > 4550 ? 255 : (g_lastVbatRest - 2000) / 10)));
      payload[1] = 0;
      payload[2] = 0;
      payload[3] = 0;
      payload[4] = 0;
      payload[5] = (uint8_t)getTemperatureC();
      payload[6] = EVENT_MAILBOX_EMPTIED;

      sendLoRaWANPacket(payload, sizeof(payload), 1);
      delay(500);
      enterDeepSleep();
    } else {
      enterDeepSleep();
    }
  }
#endif

#if DEBUG_SERIAL
  DBG_PRINTLN("[TRIGGER] Flap movement detected. Sampling flap motion profile...");
#endif

  uint32_t startTime = millis();
  float maxTilt = initial.pitch;
  float currentTilt = initial.pitch;
  uint32_t openDurationMs = 0;
  uint8_t settledCounter = 0;

  while (millis() - startTime < MAX_SAMPLE_WINDOW_MS) {
    currentTilt = getPitchAngle();
    if (currentTilt > maxTilt) {
      maxTilt = currentTilt;
    }
    if (currentTilt > 10.0f) {
      openDurationMs += 100;
      settledCounter = 0;
    } else {
      settledCounter++;
      // Exit early once the flap has returned to rest and stayed still for 400 ms.
      if (maxTilt >= TILT_TRIGGER_THRESHOLD_DEG && settledCounter >= 4) {
#if DEBUG_SERIAL
        DBG_PRINTF("[MOTION] Flap settled at rest. Exiting sample loop early at %lu ms.\n", millis() - startTime);
#endif
        break;
      }
    }
    delay(100);
  }
  float settledTilt = getPitchAngle();

#if DEBUG_SERIAL
  DBG_PRINTF("[MOTION RESULT] Peak Tilt: %.1f° | Settled Tilt: %.1f° | Open Duration: %lu ms\n",
             maxTilt, settledTilt, openDurationMs);
#endif

#if ENABLE_BLE_MAINTENANCE
  if (currentTilt >= BLE_TRIGGER_ANGLE_DEG) {
#if DEBUG_SERIAL
    DBG_PRINTF("[BLE HOLD CHECK] Flap open at %.1f° (>= %0.1f°). Checking %u second hold...\n",
               currentTilt, BLE_TRIGGER_ANGLE_DEG, BLE_HOLD_OPEN_SECONDS);
#endif
    bool heldOpen = true;
    while (millis() - startTime < ((uint32_t)BLE_HOLD_OPEN_SECONDS * 1000UL)) {
      if (getPitchAngle() < BLE_TRIGGER_ANGLE_DEG) {
        heldOpen = false;
#if DEBUG_SERIAL
        DBG_PRINTLN("[BLE HOLD CHECK] Flap released before hold timeout. Resuming normal delivery mode.");
#endif
        break;
      }
      delay(250);
    }
    if (heldOpen) {
      startBleMaintenance(BLE_TIMEOUT_SECONDS);
    }
  }
#endif

  if (maxTilt < TILT_TRIGGER_THRESHOLD_DEG) {
#if DEBUG_SERIAL
    DBG_PRINTF("[FILTER] Peak tilt %.1f° < threshold %.1f°. Ignoring false alarm.\n", maxTilt, TILT_TRIGGER_THRESHOLD_DEG);
#endif
    enterDeepSleep();
  }

  int8_t tempC = getTemperatureC();

  uint8_t payload[7];
  payload[0] = (uint8_t)((g_lastVbatRest < 2000 ? 0 : (g_lastVbatRest > 4550 ? 255 : (g_lastVbatRest - 2000) / 10)));
  payload[1] = (uint8_t)(g_lastVbatSag / 5 > 255 ? 255 : g_lastVbatSag / 5);
  payload[2] = (uint8_t)(maxTilt > 90.0f ? 90 : maxTilt);
  payload[3] = (uint8_t)(settledTilt > 90.0f ? 90 : settledTilt);
  payload[4] = (uint8_t)(openDurationMs / 100 > 255 ? 255 : openDurationMs / 100);
  payload[5] = (uint8_t)tempC;
  payload[6] = EVENT_MAIL_DELIVERED;

  sendLoRaWANPacket(payload, sizeof(payload), 1);

  delay(1000);
  enterDeepSleep();
}

void loop() {
}
