#pragma once
// USB serial command line (same commands as BLE, no PIN). Both functions are no-ops when ENABLE_SERIAL_CLI is off.

// Starts Serial if ENABLE_SERIAL_CLI and USB is connected and it is not already running.
void serialCliBegin();
// Non-blocking: collects input and runs one command per CR/LF-terminated line.
void serialCliPoll();
