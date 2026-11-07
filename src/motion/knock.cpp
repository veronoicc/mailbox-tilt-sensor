// Knock hit debouncing and the "mailbox emptied" knock pattern.
#include "motion/knock.h"
#include <Arduino.h>
#include "settings.h"
#include "core/log.h"
#include "motion/imu.h"

bool knockHit(float magG, uint32_t nowMs, uint32_t& lastHitMs) {
  if (magG > KNOCK_THRESHOLD_G && nowMs - lastHitMs > KNOCK_DEBOUNCE_MS) {
    lastHitMs = nowMs;
    return true;
  }
  return false;
}

bool knockPatternDetected() {
  uint32_t windowStart = millis();
  uint8_t knockCount = 1;
  uint32_t lastKnockTime = windowStart;

  DBG_PRINTF("[KNOCK] Initial vibration trigger! Starting %u ms pattern detection window...\n", KNOCK_WINDOW_MS);
  DBG_PRINTF("[KNOCK] Hit #1 detected | Window Start: %lu ms\n", windowStart);

  while (millis() - windowStart < KNOCK_WINDOW_MS) {
    if (knockCount == 1 && (millis() - windowStart > KNOCK_ABORT_TIMEOUT_MS)) {
      DBG_PRINTF("[KNOCK ABORT] Single transient vibration timed out after %u ms. Aborting.\n", KNOCK_ABORT_TIMEOUT_MS);
      return false;  // Abort early on single transient vibrations.
    }

    AccelSample a = imuRead();
    uint32_t now = millis();
    uint32_t delta = now - lastKnockTime;

    if (knockHit(a.mag, now, lastKnockTime)) {
      knockCount++;
      DBG_PRINTF("[KNOCK] Hit #%u detected! Peak Mag: %.2fg | Delta: %lu ms\n", knockCount, a.mag, delta);
      if (knockCount >= KNOCK_COUNT_REQUIRED) {
        DBG_PRINTF("[KNOCK PATTERN MATCHED] %u knocks confirmed! Mailbox Emptied event triggered.\n", KNOCK_COUNT_REQUIRED);
        return true;
      }
    }
    delay(20);
  }

  DBG_PRINTF("[KNOCK TIMEOUT] Window expired with only %u of %u knocks.\n", knockCount, KNOCK_COUNT_REQUIRED);
  return false;
}
