#pragma once
#include <stdint.h>
#include "settings.h"
// Awake window: BLE maintenance console (Nordic UART, newline-terminated text commands) + serial CLI.

// Runs the awake window until it closes, then stops BLE and returns. It stays open while a BLE client
// is connected; with no client it closes BLE_TIMEOUT_SECONDS after opening or after the last client left.
// It also closes when the SLEEP command ran. Bench mode keeps advertising indefinitely.
// Without ENABLE_BLE_MAINTENANCE it runs the same loop minus BLE (serial CLI + timeout).
void bleConsoleRun();

// Resets the live knock counter shown in telemetry (called by the RESET_KNOCKS command).
#if ENABLE_BLE_MAINTENANCE
void bleConsoleResetKnocks();
#else
inline void bleConsoleResetKnocks() {}
#endif
