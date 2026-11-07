#include "cmd/status.h"
#include <stdio.h>
#include "version.h"
#include "ble/console.h"
#include "core/power.h"
#include "core/state.h"
#include "lora/lorawan.h"
#include "motion/imu.h"

void statusRead(StatusData& d) {
  d.restMv = batteryRestMv();
  d.sagMv = batterySagMv();
  d.liveMv = batteryMv();
  d.tempC = temperatureC();
  AccelSample a = imuRead();
  d.imuPresent = imuPresent();
  d.pitch = a.pitch;
  d.ax = a.x;
  d.ay = a.y;
  d.az = a.z;
  d.mag = a.mag;
  d.zero = imuTiltZero();
  d.fcnt = lorawanFrameCounter();
  d.loraSession = lorawanSessionStatus();
  d.lastPayload = lorawanLastPayloadHex();
  d.knocks = bleConsoleKnockCount();
  d.bench = benchMode();
  d.usb = usbConnected();
  d.mailInside = stateMailInside();
  d.streamMs = bleConsoleStreamMs();
  d.fw = FIRMWARE_VERSION;
}

void statusText(StatusComp comp, char* buf, size_t cap) {
  StatusData d;
  statusRead(d);
  switch (comp) {
    case StatusComp::Power:
      snprintf(buf, cap, "[STATUS power] rest=%umV sag=%umV live=%umV temp=%dC", d.restMv, d.sagMv, d.liveMv, d.tempC);
      break;
    case StatusComp::Imu:
      snprintf(buf, cap, "[STATUS imu] present=%d pitch=%.1f ax=%.2f ay=%.2f az=%.2f mag=%.2f zero=%.1f",
               d.imuPresent, d.pitch, d.ax, d.ay, d.az, d.mag, d.zero);
      break;
    case StatusComp::Lora:
      snprintf(buf, cap, "[STATUS lora] fcnt=%lu session=%s payload=%s", (unsigned long)d.fcnt, d.loraSession,
               d.lastPayload);
      break;
    case StatusComp::Knock:
      snprintf(buf, cap, "[STATUS knock] count=%u", d.knocks);
      break;
    case StatusComp::System:
      snprintf(buf, cap, "[STATUS system] bench=%d usb=%d mail=%d stream_ms=%u fw=%s", d.bench, d.usb, d.mailInside,
               d.streamMs, d.fw);
      break;
  }
}
