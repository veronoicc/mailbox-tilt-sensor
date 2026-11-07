// Packs an UplinkMsg into the fixed FPort 1 frame.
#include "lora/uplink.h"

#include <math.h>
#include <string.h>

static uint8_t clampU8(long v, long hi) { return (uint8_t)(v < 0 ? 0 : (v > hi ? hi : v)); }

UplinkMsg UplinkMsg::of(UplinkReason reason) {
  UplinkMsg m;
  memset(&m, 0, sizeof m);
  m.reason = reason;
  return m;
}

UplinkMsg UplinkMsg::mailInserted(float peakDeg, float restDeg, uint32_t openMs) {
  UplinkMsg m = of(UplinkReason::MailInserted);
  m.hasTilt = true;
  m.peakTiltDeg = peakDeg;
  m.restTiltDeg = restDeg;
  m.openMs = openMs;
  return m;
}

void encodeUplink(const UplinkMsg& m, uint8_t out[UPLINK_SIZE]) {
  out[0] = (uint8_t)m.reason;
  out[1] = m.batteryMv < 2000 ? 0 : clampU8((m.batteryMv - 2000L) / 10, 255);
  out[2] = clampU8(m.sagMv / 5, 255);
  out[3] = (uint8_t)m.temperatureC;
  out[4] = m.mailInside ? 1 : 0;
  out[5] = clampU8(lroundf(m.peakTiltDeg), 90);
  out[6] = clampU8(lroundf(m.restTiltDeg), 90);
  out[7] = clampU8(m.openMs / 100, 255);
}
