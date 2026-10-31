#include <Arduino.h>
#include <Wire.h>
#include <RadioLib.h>
#include <bluefruit.h>
#include "settings.h"
#include "config.h"

#define FIRMWARE_VERSION "1.1.0"

// LoRaWAN Event Codes (Payload Byte 6)
#define EVENT_MAIL_DELIVERED 0x01
#define EVENT_MAILBOX_EMPTIED 0x02

// Nordic UART Service (NUS) for WebBluetooth Maintenance
BLEUart bleuart;

// RadioLib Driver Instance
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);
LoRaWANNode node(&radio, &LORAWAN_BAND);

// Software calibration offset for zero pitch angle
static float g_tiltZeroOffset = 0.0f;
static uint16_t g_lastVbatRest = 3000;
static uint16_t g_lastVbatSag = 0;

// Forward declarations
static void enterDeepSleep();
static void startBleMaintenance();

// ==========================================
// ROBUST HEX STRING PARSERS
// ==========================================
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

// ==========================================
// LOW-LEVEL I2C REGISTER HELPERS
// ==========================================
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

// ==========================================
// LIS3DH ACCELEROMETER INITIALIZATION
// ==========================================
void setupLIS3DH() {
  Wire.begin();

  // 1. CTRL_REG1 (0x20): Low-Power Mode
  // 0x1F = 1 Hz mode (~2 µA) if SENSOR_1HZ_ULTRA_LOW_POWER is set
  // 0x2F = 10 Hz mode (~6 µA) default
#if SENSOR_1HZ_ULTRA_LOW_POWER
  writeReg(0x20, 0x1F);
#else
  writeReg(0x20, 0x2F);
#endif

  // 2. CTRL_REG2 (0x21): High-Pass Filter with Reference Mode enabled for INT1 (0x09)
  // Essential for Anti-Jam: Measures relative motion delta rather than absolute gravity!
  writeReg(0x21, 0x09);

  // 3. CTRL_REG3 (0x22): Route IA1 to INT1 pin (0x40)
  writeReg(0x22, 0x40);

  // 4. CTRL_REG4 (0x23): +/-2g range, low power (0x00)
  writeReg(0x23, 0x00);

  // 5. INT1_THS (0x32): Threshold = 10 (~160 mg motion)
  writeReg(0x32, 0x0A);

  // 6. INT1_DURATION (0x33): 1 sample duration (0x01)
  writeReg(0x33, 0x01);

  // 7. INT1_CFG (0x30): Enable XHIE, YHIE, ZHIE interrupt generation (0x2A)
  writeReg(0x30, 0x2A);

  // Read REFERENCE register (0x26) to latch baseline gravity vector
  readReg(0x26);
}

// ==========================================
// TILT PITCH ANGLE COMPUTATION
// ==========================================
float getPitchAngle() {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(0x28 | 0x80); // Auto-increment from OUT_X_L
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)6);

  if (Wire.available() < 6) {
    return 0.0f;
  }

  int16_t x = Wire.read() | (Wire.read() << 8);
  int16_t y = Wire.read() | (Wire.read() << 8);
  int16_t z = Wire.read() | (Wire.read() << 8);

  float fx = (float)x / 16384.0f;
  float fy = (float)y / 16384.0f;
  float fz = (float)z / 16384.0f;

  float pitch = atan2(fx, sqrt(fy * fy + fz * fz)) * 180.0f / PI;
  pitch = fabs(pitch) - g_tiltZeroOffset;
  return (pitch < 0.0f) ? 0.0f : pitch;
}

// Read raw acceleration magnitude in Gs (used for knock detection)
float getAccelerationMagnitude() {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(0x28 | 0x80);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)6);

  if (Wire.available() < 6) return 1.0f;

  int16_t x = Wire.read() | (Wire.read() << 8);
  int16_t y = Wire.read() | (Wire.read() << 8);
  int16_t z = Wire.read() | (Wire.read() << 8);

  float fx = (float)x / 16384.0f;
  float fy = (float)y / 16384.0f;
  float fz = (float)z / 16384.0f;

  return sqrt(fx * fx + fy * fy + fz * fz);
}

// ==========================================
// BATTERY VOLTAGE ADC MEASUREMENT
// ==========================================
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

// Read CPU internal die temperature
int8_t getTemperatureC() {
#if ENABLE_TEMPERATURE
  return (int8_t)round(readCPUTemperature());
#else
  return 20;
#endif
}

// ==========================================
// ENTER ULTRA-LOW-POWER SLEEP (SYSTEM-OFF)
// ==========================================
static void enterDeepSleep() {
  // 1. Put SX1262 LoRa radio into cold sleep (~0.16 µA)
  radio.sleep();

  // 2. Clear LIS3DH interrupt latch by reading INT1_SRC
  readReg(0x31);

  // 3. Re-zero Reference Register: Prevents wake loops on jammed flap
  readReg(0x26);

  // 4. Ensure all LEDs are tri-stated (zero current leak)
  pinMode(LED_RED, INPUT);
  pinMode(LED_GREEN, INPUT);
  pinMode(LED_BLUE, INPUT);

  // 5. Drop into Nordic System-Off mode (~2 µA sleep floor)
  systemOff(PIN_ACCEL_INT, 1);
}

// ==========================================
// LORAWAN TRANSMISSION HELPER
// ==========================================
static bool sendLoRaWANPacket(uint8_t* payload, size_t len, uint8_t fPort) {
  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    return false;
  }

  // Slashes active LoRa TX/RX current by ~40% using the hardware DC-DC buck converter!
  radio.setRegulatorDCDC();

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
  state = node.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION && state != RADIOLIB_LORAWAN_SESSION_RESTORED && state != RADIOLIB_ERR_NONE) {
    return false;
  }

  // Transmit Uplink (Unconfirmed)
  node.sendReceive(payload, len, fPort);
  return true;
}

// ==========================================
// WEB-BLUETOOTH MAINTENANCE MODE (NUS)
// ==========================================
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
    bleuart.println("{\"status\":\"ERROR\",\"msg\":\"Authentication failed: Invalid PIN\"}");
    return;
  }

  String action = cmd.substring(cmdIdx + 4);
  action.trim();
  action.toUpperCase();

  if (action == "RECALIBRATE") {
    float rawPitch = getPitchAngle() + g_tiltZeroOffset;
    g_tiltZeroOffset = rawPitch;
    bleuart.print("{\"status\":\"OK\",\"msg\":\"Zero horizon calibrated\",\"zero_offset\":");
    bleuart.print(g_tiltZeroOffset);
    bleuart.println("}");
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
    bleuart.println("{\"status\":\"ERROR\",\"msg\":\"Unknown action. Valid: RECALIBRATE, DFU, SLEEP\"}");
  }
}

static void startBleMaintenance() {
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
  Bluefruit.Advertising.setFastTimeout(BLE_TIMEOUT_SECONDS);
  Bluefruit.Advertising.start(BLE_TIMEOUT_SECONDS);

  uint32_t bleStart = millis();
  uint32_t lastTelemetry = 0;
  String rxBuffer = "";

  while (millis() - bleStart < ((uint32_t)BLE_TIMEOUT_SECONDS * 1000UL)) {
    while (bleuart.available()) {
      char c = (char)bleuart.read();
      if (c == '\n' || c == '\r') {
        if (rxBuffer.length() > 0) {
          processBleCommand(rxBuffer);
          rxBuffer = "";
          bleStart = millis();
        }
      } else {
        rxBuffer += c;
      }
    }

    if (Bluefruit.connected() && (millis() - lastTelemetry > 1000)) {
      lastTelemetry = millis();
      float currentTilt = getPitchAngle();
      int8_t temp = getTemperatureC();
      uint16_t vbat = getBatteryMilliVolts();

      bleuart.print("{\"type\":\"telemetry\",\"v_rest\":");
      bleuart.print(g_lastVbatRest);
      bleuart.print(",\"v_sag\":");
      bleuart.print(g_lastVbatSag);
      bleuart.print(",\"v_live\":");
      bleuart.print(vbat);
      bleuart.print(",\"temp\":");
      bleuart.print(temp);
      bleuart.print(",\"tilt\":");
      bleuart.print(currentTilt, 1);
      bleuart.print(",\"fw\":\"");
      bleuart.print(FIRMWARE_VERSION);
      bleuart.println("\"}");
    }
    delay(50);
  }

  Bluefruit.disconnect(0);
  Bluefruit.Advertising.stop();
  enterDeepSleep();
}
#endif

// ==========================================
// KNOCK DETECTION ENGINE (WITH RAPID ABORT)
// ==========================================
#if ENABLE_KNOCK_DETECTION
static bool checkForKnockPattern() {
  uint32_t windowStart = millis();
  uint8_t knockCount = 1; // 1st knock already woke the interrupt
  uint32_t lastKnockTime = windowStart;

  while (millis() - windowStart < KNOCK_WINDOW_MS) {
    // If no 2nd knock within KNOCK_ABORT_TIMEOUT_MS, abort early to save battery!
    if (knockCount == 1 && (millis() - windowStart > KNOCK_ABORT_TIMEOUT_MS)) {
      return false;
    }

    float mag = getAccelerationMagnitude();
    uint32_t now = millis();

    // Shock acceleration spike > 1.4G with 160ms debounce
    if (mag > 1.4f && (now - lastKnockTime > 160)) {
      knockCount++;
      lastKnockTime = now;
      if (knockCount >= KNOCK_COUNT_REQUIRED) {
        return true;
      }
    }
    delay(20);
  }
  return false;
}
#endif

// ==========================================
// MAIN SETUP (EXECUTED ON EVERY WAKEUP)
// ==========================================
void setup() {
  // 1. Enable nRF52 internal DC-DC converter (slashes active CPU power by ~40%)
  NRF_POWER->DCDCEN = 1;

  // 2. Shut off all onboard LEDs immediately (zero passive current leak)
  pinMode(LED_RED, INPUT);
  pinMode(LED_GREEN, INPUT);
  pinMode(LED_BLUE, INPUT);

  pinMode(PIN_ACCEL_INT, INPUT);
  setupLIS3DH();

  // Read pre-transmission resting battery voltage
  g_lastVbatRest = getBatteryMilliVolts();

  // Initial tilt check
  float initialTilt = getPitchAngle();

  // ----------------------------------------------------
  // CASE 1: KNOCK PATTERN (Mailbox Emptied Detection)
  // If flap is NOT tilting (<8 deg) but shock woke the INT
  // ----------------------------------------------------
#if ENABLE_KNOCK_DETECTION
  if (initialTilt < 8.0f) {
    if (checkForKnockPattern()) {
      // 4 Knocks confirmed! Transmit Mailbox Emptied event
      uint8_t payload[7];
      payload[0] = (uint8_t)((g_lastVbatRest < 2000 ? 0 : g_lastVbatRest - 2000) / 10);
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
      // Stray vibration / door closing: Exit immediately to save power!
      enterDeepSleep();
    }
  }
#endif

  // ----------------------------------------------------
  // CASE 2: FLAP OPENING (WITH EARLY SETTLING EXIT)
  // ----------------------------------------------------
  uint32_t startTime = millis();
  float maxTilt = initialTilt;
  float currentTilt = initialTilt;
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
      // Flap returned to resting position (<10 deg)
      settledCounter++;
      // If flap has returned to rest and stayed still for 400ms, exit early!
      // This saves ~2000ms of active CPU time on normal quick envelope drops!
      if (maxTilt >= TILT_TRIGGER_THRESHOLD_DEG && settledCounter >= 4) {
        break;
      }
    }
    delay(100);
  }
  float settledTilt = getPitchAngle();

  // ----------------------------------------------------
  // CASE 3: BLE MAINTENANCE TRIGGER (Hold Open > 60° for 15s)
  // Only triggers if intentionally held wide open!
  // ----------------------------------------------------
#if ENABLE_BLE_MAINTENANCE
  if (currentTilt >= BLE_TRIGGER_ANGLE_DEG) {
    bool heldOpen = true;
    while (millis() - startTime < ((uint32_t)BLE_HOLD_OPEN_SECONDS * 1000UL)) {
      if (getPitchAngle() < BLE_TRIGGER_ANGLE_DEG) {
        heldOpen = false;
        break;
      }
      delay(250);
    }
    if (heldOpen) {
      startBleMaintenance();
    }
  }
#endif

  // ----------------------------------------------------
  // CASE 4: FALSE ALARM FILTER (< 12° opening angle)
  // ----------------------------------------------------
  if (maxTilt < TILT_TRIGGER_THRESHOLD_DEG) {
    enterDeepSleep();
  }

  // ----------------------------------------------------
  // CASE 5: MAIL DELIVERY (Flap Opened or Jammed)
  // Pack 7-Byte Telemetry Payload (FPort 1)
  // ----------------------------------------------------
  int8_t tempC = getTemperatureC();

  uint8_t payload[7];
  payload[0] = (uint8_t)((g_lastVbatRest < 2000 ? 0 : (g_lastVbatRest > 4550 ? 255 : (g_lastVbatRest - 2000) / 10)));
  payload[1] = (uint8_t)(g_lastVbatSag / 5 > 255 ? 255 : g_lastVbatSag / 5);
  payload[2] = (uint8_t)(maxTilt > 90.0f ? 90 : maxTilt);
  payload[3] = (uint8_t)(settledTilt > 90.0f ? 90 : settledTilt);
  payload[4] = (uint8_t)(openDurationMs / 100 > 255 ? 255 : openDurationMs / 100);
  payload[5] = (uint8_t)tempC;
  payload[6] = EVENT_MAIL_DELIVERED;

  // Transmit Uplink
  sendLoRaWANPacket(payload, sizeof(payload), 1);

#if ENABLE_BATTERY_SAG
  // Measure loaded battery voltage immediately after transmit pulse
  uint16_t vbatLoad = getBatteryMilliVolts();
  g_lastVbatSag = (g_lastVbatRest > vbatLoad) ? (g_lastVbatRest - vbatLoad) : 0;
#endif

  // Brief cooldown to avoid double triggers if envelope is slow
  delay(1000);

  // Return to deep sleep
  enterDeepSleep();
}

void loop() {
  // Unused: nRF52 System-Off resets to setup() on wake
}
