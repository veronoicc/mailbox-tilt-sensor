#pragma once
#include <stddef.h>
#include <stdint.h>
// FPort 1 uplink / FPort 2 downlink. Layout and scaling must match ttn/uplink_formatter.js.
// Every uplink is the same fixed frame; only the reason differs.
constexpr uint8_t UPLINK_FPORT = 1;
constexpr uint8_t DOWNLINK_FPORT = 2;
constexpr size_t UPLINK_SIZE = 8;
enum class UplinkReason : uint8_t {
  MailInserted = 0x01,
  MailCleared = 0x02,
  Heartbeat = 0x03,           // periodic timer
  HeartbeatRequested = 0x04,  // console command or downlink
  HeartbeatStartup = 0x05,
  HeartbeatPoll = 0x06,       // flap held open: poll for downlinks
  ManualTest = 0x07,
};
struct UplinkMsg {
  UplinkReason reason;
  // Filled in by lorawanSend(); callers leave them 0.
  uint16_t batteryMv;
  uint16_t sagMv;
  int8_t temperatureC;
  bool mailInside;
  // Flap data. lorawanSend() fills the current tilt (peak = rest, open = 0) unless hasTilt is set.
  bool hasTilt;
  float peakTiltDeg;
  float restTiltDeg;
  uint32_t openMs;
  static UplinkMsg of(UplinkReason reason);
  static UplinkMsg mailInserted(float peakDeg, float restDeg, uint32_t openMs);
};
void encodeUplink(const UplinkMsg& m, uint8_t out[UPLINK_SIZE]);
