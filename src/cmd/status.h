#pragma once
#include <stddef.h>
#include <stdint.h>
// Device status snapshot shared by every command sink (BLE frames, serial text, debug log).

// Values match the wire component ids in ble/proto.h.
enum class StatusComp : uint8_t { Power = 1, Imu = 2, Lora = 3, Knock = 4, System = 5 };

struct StatusData {
  uint16_t restMv, sagMv, liveMv;
  int8_t tempC;
  bool imuPresent;
  float pitch, ax, ay, az, mag, zero;  // degrees / g
  uint32_t fcnt;
  const char* loraSession;  // static strings owned by the LoRaWAN module
  const char* lastPayload;
  uint16_t knocks;
  bool bench, usb, mailInside;
  uint16_t streamMs;
  const char* fw;
};

void statusRead(StatusData& d);
// One human-readable line, no newline: "[STATUS imu] present=1 pitch=...". Reads fresh data.
void statusText(StatusComp comp, char* buf, size_t cap);
