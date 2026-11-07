// Packs an UplinkMsg into the tagged FPort 1 frame.
#include "lora/uplink.h"

#include <math.h>
#include <string.h>

static uint8_t clampU8(long v, long hi) { return (uint8_t)(v < 0 ? 0 : (v > hi ? hi : v)); }

static UplinkMsg make(MsgType t) {
  UplinkMsg m;
  memset(&m, 0, sizeof m);
  m.type = t;
  return m;
}

UplinkMsg UplinkMsg::mailInserted(float peakDeg, float restDeg, uint32_t openMs) {
  UplinkMsg m = make(MsgType::MailInserted);
  m.flap = {peakDeg, restDeg, openMs};
  return m;
}

UplinkMsg UplinkMsg::manualTest(float tiltDeg, float zeroDeg) {
  UplinkMsg m = make(MsgType::ManualTest);
  m.flap = {tiltDeg, zeroDeg, 0};
  return m;
}

UplinkMsg UplinkMsg::mailCleared(ClearSource source) {
  UplinkMsg m = make(MsgType::MailCleared);
  m.cleared.source = source;
  return m;
}

UplinkMsg UplinkMsg::heartbeat(bool mailInside, HeartbeatReason reason) {
  UplinkMsg m = make(MsgType::Heartbeat);
  m.hb = {mailInside, reason};
  return m;
}

size_t encodeUplink(const UplinkMsg& m, uint8_t out[UPLINK_MAX_SIZE]) {
  out[0] = (uint8_t)m.type;
  out[1] = m.batteryMv < 2000 ? 0 : clampU8((m.batteryMv - 2000L) / 10, 255);
  out[2] = clampU8(m.sagMv / 5, 255);
  out[3] = (uint8_t)m.temperatureC;
  switch (m.type) {
    case MsgType::MailInserted:
    case MsgType::ManualTest:
      out[4] = clampU8(lroundf(m.flap.peakTiltDeg), 90);
      out[5] = clampU8(lroundf(m.flap.restTiltDeg), 90);
      out[6] = clampU8(m.flap.openMs / 100, 255);
      return 7;
    case MsgType::MailCleared:
      out[4] = (uint8_t)m.cleared.source;
      return 5;
    case MsgType::Heartbeat:
      out[4] = m.hb.mailInside ? 1 : 0;
      out[5] = (uint8_t)m.hb.reason;
      return 6;
  }
  return 4;
}
