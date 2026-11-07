// System-ON sleep with an INT1 wake and a millisecond timeout.
#include "core/sleep.h"
#include "settings.h"

#if HEARTBEAT_INTERVAL_HOURS > 0
#include <Arduino.h>
#include "semphr.h"
#include "board.h"

static SemaphoreHandle_t g_sem = nullptr;

static void onInt1() {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(g_sem, &woken);
  portYIELD_FROM_ISR(woken);
}

SleepWake sleepWait(uint32_t timeoutMs) {
  if (!g_sem) {
    g_sem = xSemaphoreCreateBinary();
    // ponytail: the framework's attachInterrupt uses a GPIOTE IN-event channel, which Nordic reports
    // costs a few µA more in System-On idle than a PORT/SENSE wake. RadioLib also relies on the
    // framework's GPIOTE ISR for DIO1, so a custom PORT handler would have to replace both.
    // Switch to PORT only if measured idle current matters.
    attachInterrupt(PIN_ACCEL_INT, onInt1, RISING);
  }

  xSemaphoreTake(g_sem, 0);  // drop a stale give from an earlier wake
  // A line already high gives no new rising edge.
  if (digitalRead(PIN_ACCEL_INT)) return SleepWake::Motion;

  return xSemaphoreTake(g_sem, pdMS_TO_TICKS(timeoutMs)) == pdTRUE ? SleepWake::Motion : SleepWake::Timeout;
}
#endif
