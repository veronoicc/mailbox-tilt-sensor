// Mailbox tilt sensor. The LIS3DH INT1 pin wakes the MCU from System-Off (a full reset);
// setup() handles one event and powers off again, so loop() never runs.
#include <Arduino.h>
#include "board.h"
#include "settings.h"
#include "version.h"
#include "core/log.h"
#include "core/power.h"
#include "motion/imu.h"
#include "motion/knock.h"
#include "motion/flap.h"
#include "lora/lorawan.h"
#include "ble/console.h"

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

// Blink Blue LED on power-up so the user immediately sees the board is running.
static void blinkBoot() {
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, XIAO_LED_ON);
  delay(100);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);
  delay(80);
  digitalWrite(LED_BLUE, XIAO_LED_ON);
  delay(100);
  digitalWrite(LED_BLUE, XIAO_LED_OFF);
}

static void startSerial() {
  if (!DEBUG_SERIAL) return;
  Serial.begin(115200);
  // Wait up to 2.5s for Serial Monitor if USB is plugged in
  uint32_t t0 = millis();
  while (!Serial && usbConnected() && (millis() - t0 < 2500)) {
    delay(10);
  }
  DBG_PRINTLN("\n==================================================");
  DBG_PRINTF("   MAILBOX TILT SENSOR [FW v%s]\n", FIRMWARE_VERSION);
  DBG_PRINTF("   nRF52840 + SX1262 (%s) + LIS3DH 3-Axis Accel\n", STRINGIFY(LORAWAN_BAND));
  DBG_PRINTF("   Power: %s\n", usbConnected() ? "USB 5V (Bench Test Mode)" : "CR123A Battery 3V");
  DBG_PRINTLN("==================================================");
}

[[noreturn]] static void sleepUntilWake() {
  if (benchMode()) {
    DBG_PRINTLN("\n[BENCH MODE] Device will not deep-sleep: USB is plugged in or IMU is absent.");
    DBG_PRINTLN("[BENCH MODE] Continuing WebBluetooth maintenance mode...");
  }
  while (benchMode()) {
#if ENABLE_BLE_MAINTENANCE
    bleConsoleRun(BLE_IDLE_TIMEOUT_SECONDS);
#else
    watchdogFeed();
    delay(1000);
#endif
  }

  DBG_PRINTLN("[POWER] Entering System-Off Deep Sleep (wake on INT1 pin)...");
  if (DEBUG_SERIAL) Serial.flush();

  lorawanSleep();
  imuArmWake();

  // Tri-state LEDs to prevent active-low parasitic draw.
  pinMode(LED_RED, INPUT);
  pinMode(LED_GREEN, INPUT);
  pinMode(LED_BLUE, INPUT);

  systemOff(PIN_ACCEL_INT, 1);
  for (;;) {}
}

void setup() {
  watchdogStart();
  blinkBoot();
  startSerial();

  const bool woke = wokeFromSystemOff();
  (void)woke;  // only used when BLE maintenance is enabled
  dcdcEnable();

  pinMode(PIN_ACCEL_INT, INPUT);
  imuBegin();
  batterySampleRest();
  const AccelSample initial = imuRead();

  DBG_PRINTF("[INIT] Battery Rest Voltage: %u mV\n", batteryRestMv());
  DBG_PRINTF("[INIT] Internal Temp:        %d °C\n", temperatureC());
  DBG_PRINTF("[INIT] LIS3DH Status:        %s\n", imuPresent() ? "CONNECTED (OK)" : "DISCONNECTED (Bench Mode)");
  if (imuPresent()) {
    DBG_PRINTF("[INIT] LIS3DH Accel:         X=%.2fg, Y=%.2fg, Z=%.2fg (Mag=%.2fg)\n",
               initial.x, initial.y, initial.z, initial.mag);
    DBG_PRINTF("[INIT] Initial Pitch Angle:  %.1f deg (Zero offset: %.1f deg)\n", initial.pitch, imuTiltZero());
  }

#if ENABLE_BLE_MAINTENANCE
  // On cold boot/power-up, or if USB is plugged in or the IMU is absent:
  if (!woke || benchMode()) {
    DBG_PRINTF("[BOOT] Advertising WebBluetooth as '%s'...\n", BLE_DEVICE_NAME);
    bleConsoleRun(BLE_BOOT_ADVERTISE_SECONDS);
    sleepUntilWake();
  }
#endif

  // If IMU is missing, enter safe idle rather than waking on noise
  if (!imuPresent()) {
    DBG_PRINTLN("[HALT] IMU not detected. Waiting in bench mode...");
    sleepUntilWake();
  }

#if ENABLE_KNOCK_DETECTION
  // When an interrupt fires without tilt, check if it matches a knock pattern.
  if (initial.pitch < FLAP_CLOSED_DEG) {
    DBG_PRINTLN("[TRIGGER] Accel INT1 fired while flap is closed (<8 deg). Checking knock pattern...");
    if (knockPatternDetected()) {
      lorawanSendEvent(UplinkEvent::MailboxEmptied, 0, 0, 0);
      delay(500);
    }
    sleepUntilWake();
  }
#endif

  const FlapMotion m = flapSample(initial.pitch);

#if ENABLE_BLE_MAINTENANCE
  if (flapHeldOpen(m)) {
    bleConsoleRun(BLE_IDLE_TIMEOUT_SECONDS);
    sleepUntilWake();
  }
#endif

  if (m.peakDeg < FLAP_TRIGGER_DEG) {
    DBG_PRINTF("[FILTER] Peak tilt %.1f° < threshold %.1f°. Ignoring false alarm.\n", m.peakDeg, FLAP_TRIGGER_DEG);
    sleepUntilWake();
  }

  lorawanSendEvent(UplinkEvent::MailDelivered, m.peakDeg, m.settledDeg, m.openMs);
  delay(1000);
  sleepUntilWake();
}

void loop() {}
