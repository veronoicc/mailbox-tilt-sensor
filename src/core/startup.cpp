#include "core/startup.h"

static constexpr uint32_t STARTUP_MAGIC = 0x5C0DE5A1;
__attribute__((section(".noinit"))) static uint32_t g_startupMagic;
__attribute__((section(".noinit"))) static uint8_t g_startupCount;

uint8_t startupCounterRecord(bool realStartup) {
  if (!realStartup) return 0;
  if (g_startupMagic != STARTUP_MAGIC) {
    g_startupMagic = STARTUP_MAGIC;
    g_startupCount = 0;
  }
  if (g_startupCount < 255) g_startupCount++;
  return g_startupCount;
}

void startupCounterClear() {
  g_startupMagic = 0;
  g_startupCount = 0;
}
