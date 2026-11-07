#pragma once
#include <stddef.h>
#include <stdint.h>
// FPort 1 uplink. Layout and scaling must match ttn/payload_formatter.js.
constexpr uint8_t UPLINK_FPORT = 1;
constexpr size_t UPLINK_SIZE = 7;
enum class UplinkEvent : uint8_t { MailDelivered = 0x01, MailboxEmptied = 0x02, ManualTest = 0x03 };
struct UplinkData {
  uint16_t batteryMv;   // resting battery voltage
  uint16_t sagMv;       // drop measured after the previous TX (0 if none)
  float peakTiltDeg;
  float restTiltDeg;
  uint32_t openMs;
  int8_t temperatureC;
  UplinkEvent event;
};
void encodeUplink(const UplinkData& d, uint8_t out[UPLINK_SIZE]);
