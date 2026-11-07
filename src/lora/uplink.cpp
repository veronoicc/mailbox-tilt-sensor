// Packs an UplinkData into the 7-byte FPort 1 frame.
#include "lora/uplink.h"

#include <math.h>

static uint8_t clampU8(long v, long hi) { return (uint8_t)(v < 0 ? 0 : (v > hi ? hi : v)); }

void encodeUplink(const UplinkData& d, uint8_t out[UPLINK_SIZE]) {
  out[0] = d.batteryMv < 2000 ? 0 : clampU8((d.batteryMv - 2000L) / 10, 255);
  out[1] = clampU8(d.sagMv / 5, 255);
  out[2] = clampU8(lroundf(d.peakTiltDeg), 90);
  out[3] = clampU8(lroundf(d.restTiltDeg), 90);
  out[4] = clampU8(d.openMs / 100, 255);
  out[5] = (uint8_t)d.temperatureC;
  out[6] = (uint8_t)d.event;
}
