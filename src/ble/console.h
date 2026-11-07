#pragma once
#include <stdint.h>
#include "settings.h"
// Awake window: BLE maintenance console (Nordic UART, binary device->browser frames, see ble/proto.h)
// plus the serial CLI.

// Runs the awake window until it closes, then stops BLE and returns. It stays open while a BLE client
// is connected; with no client it closes BLE_TIMEOUT_SECONDS after opening or after the last client left.
// It also closes when the SLEEP command ran. Bench mode keeps advertising indefinitely.
// Without ENABLE_BLE_MAINTENANCE it runs the same loop minus BLE (serial CLI + timeout).
void bleConsoleRun();

// Live knock counter and telemetry stream interval (0 = off). The interval resets to
// BLE_STREAM_DEFAULT_MS on every new connection.
#if ENABLE_BLE_MAINTENANCE
void bleConsoleResetKnocks();
uint16_t bleConsoleKnockCount();
uint16_t bleConsoleStreamMs();
void bleConsoleSetStreamMs(uint16_t ms);
#else
inline void bleConsoleResetKnocks() {}
inline uint16_t bleConsoleKnockCount() { return 0; }
inline uint16_t bleConsoleStreamMs() { return 0; }
inline void bleConsoleSetStreamMs(uint16_t) {}
#endif
