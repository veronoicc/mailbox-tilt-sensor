// Mailbox tilt sensor. With HEARTBEAT_INTERVAL_HOURS == 0 the LIS3DH INT1 pin wakes the MCU from
// System-Off (a full reset) and setup() handles one event, so loop() never runs. Otherwise the MCU
// sleeps in System-ON and loop() is the wake loop.
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
#include "cmd/cmd.h"
#include "core/heartbeat_policy.h"
#include "core/sleep.h"
#include "core/startup.h"
#include "core/state.h"

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

// Downlink-triggered commands have no console to answer; their output goes to the debug log.
static void logReply(void*, bool ok, const char* msg) { DBG_PRINTF("[%s] %s\n", ok ? "OK" : "ERROR", msg); }

static void logStatus(void*, StatusComp comp) {
  if (!DEBUG_SERIAL) return;
  char line[160];
  statusText(comp, line, sizeof line);
  Serial.println(line);
}

static const CmdOut kLogOut = {logReply, logStatus, nullptr};

// Stay awake in the BLE/serial window for as long as USB is plugged in or the IMU is absent.
static void holdWhileBench() {
  if (!benchMode()) return;
  DBG_PRINTLN("\n[BENCH MODE] Device will not sleep: USB is plugged in or IMU is absent.");
  DBG_PRINTLN("[BENCH MODE] Continuing maintenance mode...");
  while (benchMode()) {
    bleConsoleRun();
    cmdClearSleepRequest();
    watchdogFeed();
  }
}

// Everything that must be quiet before the MCU sleeps.
static void prepareSleep() {
  if (DEBUG_SERIAL) Serial.flush();
  lorawanSleep();
  // Tri-state LEDs to prevent active-low parasitic draw.
  pinMode(LED_RED, INPUT);
  pinMode(LED_GREEN, INPUT);
  pinMode(LED_BLUE, INPUT);
}

// One INT1 event: knock pattern on a closed flap, hold-open maintenance window, or a flap delivery.
static void handleWake(const AccelSample& initial) {
#if ENABLE_KNOCK_DETECTION
  // When an interrupt fires without tilt, check if it matches a knock pattern.
  if (initial.pitch < FLAP_CLOSED_DEG) {
    DBG_PRINTLN("[TRIGGER] Accel INT1 fired while flap is closed (<8 deg). Checking knock pattern...");
    if (knockPatternDetected()) {
      stateSetMailInside(false);
      lorawanSend(UplinkMsg::mailCleared(ClearSource::Knock));
      cmdDrainDownlinks(kLogOut);
    }
    return;
  }
#endif

  const FlapMotion m = flapSample(initial.pitch);

  if (flapHeldOpen(m)) {
    lorawanSend(UplinkMsg::heartbeat(stateMailInside(), HeartbeatReason::Poll));
    cmdDrainDownlinks(kLogOut);
    if (!cmdSleepRequested()) bleConsoleRun();
    cmdClearSleepRequest();
    return;
  }

  if (m.peakDeg < FLAP_TRIGGER_DEG) {
    DBG_PRINTF("[FILTER] Peak tilt %.1f° < threshold %.1f°. Ignoring false alarm.\n", m.peakDeg, FLAP_TRIGGER_DEG);
    return;
  }

  stateSetMailInside(true);  // physical truth, even if the uplink fails
  lorawanSend(UplinkMsg::mailInserted(m.peakDeg, m.settledDeg, m.openMs));
  cmdDrainDownlinks(kLogOut);
}

#if HEARTBEAT_INTERVAL_HOURS > 0
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = HEARTBEAT_INTERVAL_HOURS * 3600000UL;
static_assert(HEARTBEAT_INTERVAL_HOURS <= 500, "HEARTBEAT_INTERVAL_HOURS must keep the interval below 2^31 ms (signed delta)");
// Reset after the startup heartbeat and after each timer heartbeat only, so frequent mail events
// cannot starve it. Compared as a signed delta so it survives the millis() wrap.
static uint32_t g_nextHeartbeatMs;
#endif

void setup() {
  watchdogStart();
  blinkBoot();
  startSerial();

  const bool woke = wokeFromSystemOff();
  const uint8_t startups = startupCounterRecord(!woke);
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

  if (heartbeatOnStartup(HEARTBEAT_ON_STARTUP, !woke, startups, HEARTBEAT_RAPID_REBOOT_MAX)) {
    lorawanSend(UplinkMsg::heartbeat(stateMailInside(), HeartbeatReason::Startup));
    cmdDrainDownlinks(kLogOut);
  }
#if HEARTBEAT_INTERVAL_HOURS > 0
  g_nextHeartbeatMs = millis() + HEARTBEAT_INTERVAL_MS;
#endif

  if (!woke || benchMode()) {
    DBG_PRINTLN("[BOOT] Opening the awake window...");
    bleConsoleRun();
    cmdClearSleepRequest();
  } else {
    handleWake(initial);
  }
  holdWhileBench();

#if HEARTBEAT_INTERVAL_HOURS == 0
  DBG_PRINTLN("[POWER] Entering System-Off Deep Sleep (wake on INT1 pin)...");
  prepareSleep();
  imuArmWake();
  startupCounterClear();
  systemOff(PIN_ACCEL_INT, 1);
  for (;;) {}
#endif
}

#if HEARTBEAT_INTERVAL_HOURS > 0
// System-ON wake loop: the CPU sleeps in FreeRTOS tickless idle (RTC1 keeps time) until INT1 fires
// or the heartbeat is due, so a flap wake is not a reset. The SoftDevice stays enabled after the
// first BLE use and keeps its own low-power clock running, which costs extra current compared with
// System-Off; the amount is unmeasured. WDT_CONFIG_SLEEP_Pause pauses the watchdog while the CPU
// sleeps, so a long sleep does not reset the device; it is fed on every wake.
void loop() {
  watchdogFeed();
  holdWhileBench();

  int32_t left = (int32_t)(g_nextHeartbeatMs - millis());
  if (left <= 0) {
    lorawanSend(UplinkMsg::heartbeat(stateMailInside(), HeartbeatReason::Timer));
    cmdDrainDownlinks(kLogOut);
    g_nextHeartbeatMs = millis() + HEARTBEAT_INTERVAL_MS;
    left = (int32_t)HEARTBEAT_INTERVAL_MS;
  }

  prepareSleep();
  imuArmWake();  // release the latched INT1 line before arming, or a high line gives no new event
  startupCounterClear();
  const SleepWake why = sleepWait((uint32_t)left);
  watchdogFeed();
  if (why == SleepWake::Motion) handleWake(imuRead());
}
#else
void loop() {}
#endif
