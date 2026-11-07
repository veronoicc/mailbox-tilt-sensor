// Flap open/close profiling and the BLE "hold open" gesture.
#include "motion/flap.h"
#include <Arduino.h>
#include "settings.h"
#include "core/log.h"
#include "motion/imu.h"

FlapMotion flapSample(float initialDeg) {
  DBG_PRINTLN("[TRIGGER] Flap movement detected. Sampling flap motion profile...");

  FlapMotion m = {millis(), initialDeg, initialDeg, 0.0f, 0};
  uint8_t settledCounter = 0;

  while (millis() - m.startMs < FLAP_SAMPLE_WINDOW_MS) {
    m.lastDeg = imuRead().pitch;
    if (m.lastDeg > m.peakDeg) {
      m.peakDeg = m.lastDeg;
    }
    if (m.lastDeg > FLAP_OPEN_DEG) {
      m.openMs += 100;
      settledCounter = 0;
    } else {
      settledCounter++;
      // Exit early once the flap has returned to rest and stayed still for 400 ms.
      if (m.peakDeg >= FLAP_TRIGGER_DEG && settledCounter >= 4) {
        DBG_PRINTF("[MOTION] Flap settled at rest. Exiting sample loop early at %lu ms.\n", millis() - m.startMs);
        break;
      }
    }
    delay(100);
  }
  m.settledDeg = imuRead().pitch;

  DBG_PRINTF("[MOTION RESULT] Peak Tilt: %.1f° | Settled Tilt: %.1f° | Open Duration: %lu ms\n",
             m.peakDeg, m.settledDeg, m.openMs);
  return m;
}

bool flapHeldOpen(const FlapMotion& m) {
  if (m.lastDeg < BLE_HOLD_ANGLE_DEG) return false;

  DBG_PRINTF("[BLE HOLD CHECK] Flap open at %.1f° (>= %0.1f°). Checking %u second hold...\n",
             m.lastDeg, BLE_HOLD_ANGLE_DEG, BLE_HOLD_SECONDS);
  while (millis() - m.startMs < (uint32_t)BLE_HOLD_SECONDS * 1000UL) {
    if (imuRead().pitch < BLE_HOLD_ANGLE_DEG) {
      DBG_PRINTLN("[BLE HOLD CHECK] Flap released before hold timeout. Resuming normal delivery mode.");
      return false;
    }
    delay(250);
  }
  return true;
}
