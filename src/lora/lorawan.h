#pragma once
#include <stddef.h>
#include <stdint.h>
#include "lora/uplink.h"
// LoRaWAN OTAA join/restore and uplink on the SX1262.

// Fills the common header (battery, sag, temperature), encodes, joins (or restores the session) and transmits on FPort 1.
bool lorawanSend(UplinkMsg msg);
// Returns true once per downlink received in the RX windows of the last send, then clears it.
// len is truncated to cap; the stored downlink is at most 16 bytes.
bool lorawanTakeDownlink(uint8_t& fPort, uint8_t* buf, size_t cap, size_t& len);
uint32_t lorawanFrameCounter();
// "COLD_BOOT" | "RESTORED" | "NEW_JOINED" | "ACTIVE" | "FAILED"
const char* lorawanSessionStatus();
// Hex of the last payload sent, "None" until the first send.
const char* lorawanLastPayloadHex();
void lorawanSleep();
