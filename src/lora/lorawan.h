#pragma once
#include <stdint.h>
#include "lora/uplink.h"
// LoRaWAN OTAA join/restore and uplink on the SX1262.

// Joins (or restores the session), builds the FPort 1 uplink and transmits it.
bool lorawanSendEvent(UplinkEvent event, float peakTiltDeg, float restTiltDeg, uint32_t openMs);
uint32_t lorawanFrameCounter();
// "COLD_BOOT" | "RESTORED" | "NEW_JOINED" | "ACTIVE" | "FAILED"
const char* lorawanSessionStatus();
// Hex of the last payload sent, "None" until the first send.
const char* lorawanLastPayloadHex();
void lorawanSleep();
