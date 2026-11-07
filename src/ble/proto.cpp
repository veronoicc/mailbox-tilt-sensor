#include "ble/proto.h"
#include <math.h>
#include <string.h>

uint8_t protoCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  while (len--) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

// Payload builder writing straight into the frame buffer after the 3-byte header.
struct Pl {
  uint8_t* b;
  size_t n = 0;
  explicit Pl(uint8_t* buf) : b(buf + 3) {}
  void u8(uint8_t v) { b[n++] = v; }
  void u16(uint16_t v) { u8(v & 0xFF); u8(v >> 8); }
  void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
  // Raw bytes, capped at max.
  size_t raw(const char* s, size_t max) {
    size_t l = strnlen(s, max);
    memcpy(b + n, s, l);
    n += l;
    return l;
  }
  // Length-prefixed string.
  void str(const char* s, size_t max) {
    size_t at = n++;
    b[at] = (uint8_t)raw(s, max);
  }
};

static size_t seal(uint8_t* buf, uint8_t type, size_t len) {
  buf[0] = PROTO_SYNC;
  buf[1] = type;
  buf[2] = (uint8_t)len;
  buf[3 + len] = protoCrc8(buf + 1, len + 2);
  return len + 4;
}

int16_t protoI16(float v) {
  if (v >= 32767.0f) return 32767;
  if (v <= -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

uint16_t protoU16(float v) {
  if (v >= 65535.0f) return 65535;
  if (v <= 0.0f) return 0;
  return (uint16_t)lroundf(v);
}

size_t protoTelemetry(uint8_t* buf, uint16_t v, int8_t t, int16_t pitch, int16_t ax, int16_t ay,
                      int16_t az, uint16_t mag, uint16_t knocks, bool mail) {
  Pl p(buf);
  p.u16(v); p.u8((uint8_t)t); p.u16(pitch); p.u16(ax); p.u16(ay); p.u16(az); p.u16(mag); p.u16(knocks);
  p.u8(mail ? 1 : 0);
  return seal(buf, PROTO_TELEMETRY, p.n);
}

size_t protoKnock(uint8_t* buf, uint16_t count, uint16_t mag) {
  Pl p(buf);
  p.u16(count); p.u16(mag);
  return seal(buf, PROTO_KNOCK, p.n);
}

size_t protoReply(uint8_t* buf, bool ok, const char* msg) {
  Pl p(buf);
  p.u8(ok ? 1 : 0);
  p.raw(msg, PROTO_MAX_REPLY);
  return seal(buf, PROTO_REPLY, p.n);
}

static Pl status(uint8_t* buf, uint8_t comp) {
  Pl p(buf);
  p.u8(comp);
  return p;
}

size_t protoStatusPower(uint8_t* buf, uint16_t rest, uint16_t sag, uint16_t live, int8_t t) {
  Pl p = status(buf, COMP_POWER);
  p.u16(rest); p.u16(sag); p.u16(live); p.u8((uint8_t)t);
  return seal(buf, PROTO_STATUS, p.n);
}

size_t protoStatusImu(uint8_t* buf, bool present, int16_t pitch, int16_t ax, int16_t ay, int16_t az,
                      uint16_t mag, int16_t zero) {
  Pl p = status(buf, COMP_IMU);
  p.u8(present ? 1 : 0); p.u16(pitch); p.u16(ax); p.u16(ay); p.u16(az); p.u16(mag); p.u16(zero);
  return seal(buf, PROTO_STATUS, p.n);
}

size_t protoStatusLora(uint8_t* buf, uint32_t fcnt, const char* session, const char* payloadHex) {
  Pl p = status(buf, COMP_LORA);
  p.u32(fcnt);
  p.str(session, 24);
  p.str(payloadHex, 32);  // 1+4+1+24+1+32 = 63 <= 64
  return seal(buf, PROTO_STATUS, p.n);
}

size_t protoStatusKnock(uint8_t* buf, uint16_t count) {
  Pl p = status(buf, COMP_KNOCK);
  p.u16(count);
  return seal(buf, PROTO_STATUS, p.n);
}

size_t protoStatusSystem(uint8_t* buf, bool bench, bool usb, bool mail, uint16_t streamMs, const char* fw) {
  Pl p = status(buf, COMP_SYSTEM);
  p.u8(bench ? 1 : 0); p.u8(usb ? 1 : 0); p.u8(mail ? 1 : 0); p.u16(streamMs);
  p.str(fw, 32);
  return seal(buf, PROTO_STATUS, p.n);
}
