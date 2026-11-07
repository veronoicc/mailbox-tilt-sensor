// Power, reset, watchdog, battery and temperature helpers.
#include "core/power.h"
#include <Arduino.h>
#include <math.h>
#include "settings.h"
#include "core/log.h"
#include "motion/imu.h"

static uint16_t g_restMv = 3000;
static uint16_t g_sagMv = 0;

bool usbConnected() {
  return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}

bool benchMode() {
  return usbConnected() || !imuPresent();
}

bool wokeFromSystemOff() {
  uint32_t reason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = 0xFFFFFFFF;  // Clear reset flags
  bool woke = (reason & POWER_RESETREAS_OFF_Msk) != 0;
  DBG_PRINTF("[BOOT] Reset Reason: 0x%08X (WakeFromSleep=%s)\n", (unsigned)reason, woke ? "YES" : "NO");
  return woke;
}

void dcdcEnable() {
  NRF_POWER->DCDCEN = 1;  // Lower CPU active draw.
}

void watchdogStart() {
#if ENABLE_WATCHDOG
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Pause << WDT_CONFIG_SLEEP_Pos);
  NRF_WDT->CRV = (WATCHDOG_TIMEOUT_SECONDS * 32768UL) - 1;
  NRF_WDT->RREN = (WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos);
  NRF_WDT->TASKS_START = 1;
  watchdogFeed();
#endif
}

void watchdogFeed() {
#if ENABLE_WATCHDOG
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
#endif
}

uint16_t batteryMv() {
  analogReference(AR_INTERNAL);  // 0.6 V reference x 6 = 3.6 V full scale
  analogReadResolution(12);
  // The CR123A feeds the 3V3 rail directly, so VDD is the battery voltage.
  return (uint16_t)(analogReadVDD() * 3600UL / 4096UL);
}

void batterySampleRest() {
  g_restMv = batteryMv();
}

uint16_t batteryRestMv() {
  return g_restMv;
}

uint16_t batterySampleSag() {
  uint16_t load = batteryMv();
  g_sagMv = (g_restMv > load) ? (g_restMv - load) : 0;
  return g_sagMv;
}

uint16_t batterySagMv() {
  return g_sagMv;
}

int8_t temperatureC() {
#if ENABLE_TEMPERATURE
  return (int8_t)lroundf(readCPUTemperature());
#else
  return 20;
#endif
}
