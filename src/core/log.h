#pragma once
#include <Arduino.h>
// Serial debug output. DEBUG_SERIAL (0/1) is set per PlatformIO env; with 0 the calls
// compile away but their arguments are still type-checked.
#define DBG_PRINTLN(...) do { if (DEBUG_SERIAL) Serial.println(__VA_ARGS__); } while (0)
#define DBG_PRINTF(...)  do { if (DEBUG_SERIAL) Serial.printf(__VA_ARGS__); } while (0)
