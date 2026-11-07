#pragma once
#include <stdint.h>
// True if a heartbeat should be sent right after this startup. realStartup = reset was NOT a System-Off flap wake.
// consecutiveStartups counts this startup (the first startup is 1).
bool heartbeatOnStartup(bool enabled, bool realStartup, uint8_t consecutiveStartups, uint8_t rapidRebootMax);
