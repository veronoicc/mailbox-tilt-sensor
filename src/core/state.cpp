#include "core/state.h"
#include <stdint.h>
#include "core/store.h"

static const char* STATE_FILE = "/state";
static bool g_loaded = false;
static bool g_mailInside = false;

static void stateLoad() {
  if (g_loaded) return;
  uint8_t b = 0;
  g_mailInside = storeRead(STATE_FILE, &b, 1) && b == 1;
  g_loaded = true;
}

bool stateMailInside() {
  stateLoad();
  return g_mailInside;
}

void stateSetMailInside(bool inside) {
  stateLoad();
  if (inside == g_mailInside) return;
  uint8_t b = inside ? 1 : 0;
  if (storeWrite(STATE_FILE, &b, 1)) g_mailInside = inside;
}
