#pragma once
#include <stddef.h>
#include <stdint.h>
// FPort 1 uplink / FPort 2 downlink. Layout and scaling must match ttn/payload_formatter.js.
constexpr uint8_t UPLINK_FPORT = 1;
constexpr uint8_t DOWNLINK_FPORT = 2;
constexpr size_t UPLINK_MAX_SIZE = 8;
enum class MsgType : uint8_t { MailInserted = 0x01, MailCleared = 0x02, Heartbeat = 0x03, ManualTest = 0x04 };
enum class ClearSource : uint8_t { Knock = 0, Downlink = 1, Console = 2 };
enum class HeartbeatReason : uint8_t { Timer = 0, Startup = 1, Requested = 2, Poll = 3 };
struct UplinkMsg {
  MsgType type;
  uint16_t batteryMv;  // common header; filled in by lorawanSend(), callers leave 0
  uint16_t sagMv;
  int8_t temperatureC;
  union {
    struct { float peakTiltDeg; float restTiltDeg; uint32_t openMs; } flap;  // MailInserted, ManualTest
    struct { ClearSource source; } cleared;                                  // MailCleared
    struct { bool mailInside; HeartbeatReason reason; } hb;                  // Heartbeat ("heartbeat" is the factory name)
  };
  static UplinkMsg mailInserted(float peakDeg, float restDeg, uint32_t openMs);
  static UplinkMsg manualTest(float tiltDeg, float zeroDeg);  // peak=tilt, rest=zero offset, open=0
  static UplinkMsg mailCleared(ClearSource source);
  static UplinkMsg heartbeat(bool mailInside, HeartbeatReason reason);
};
size_t encodeUplink(const UplinkMsg& m, uint8_t out[UPLINK_MAX_SIZE]);  // returns bytes written
