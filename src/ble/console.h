#pragma once
#include <stdint.h>
// Web Bluetooth maintenance console (Nordic UART service, newline-terminated text commands).

// Runs the Web Bluetooth maintenance session until no client has been connected for
// idleTimeoutS (never in bench mode) or a SLEEP command arrives, then stops BLE and returns.
void bleConsoleRun(uint32_t idleTimeoutS);
