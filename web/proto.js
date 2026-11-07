"use strict";

// Binary frame protocol (device -> browser). Mirrors src/ble/proto.{h,cpp}.
// Frame: 0xA5, type, len (<=64), payload[len], crc8(type,len,payload) poly 0x07 init 0.
// Little-endian payloads.

const PROTO_SYNC = 0xa5;
const PROTO_MAX_PAYLOAD = 64;
const PROTO_TYPE = { TELEMETRY: 1, KNOCK: 2, REPLY: 3, STATUS: 4 };

function protoCrc8(bytes) {
  let crc = 0;
  for (const b of bytes) {
    crc ^= b;
    for (let i = 0; i < 8; i++) crc = crc & 0x80 ? ((crc << 1) ^ 0x07) & 0xff : (crc << 1) & 0xff;
  }
  return crc;
}

function protoDecodePayload(type, p) {
  const dv = new DataView(p.buffer, p.byteOffset, p.byteLength);
  const ascii = (off, n) => String.fromCharCode(...p.subarray(off, off + n));
  switch (type) {
    case PROTO_TYPE.TELEMETRY:
      if (p.length !== 16) return null;
      return {
        type: "telemetry",
        v_live: dv.getUint16(0, true), temp: dv.getInt8(2),
        pitch: dv.getInt16(3, true) / 10,
        ax: dv.getInt16(5, true) / 1000, ay: dv.getInt16(7, true) / 1000, az: dv.getInt16(9, true) / 1000,
        mag: dv.getUint16(11, true) / 1000, knocks: dv.getUint16(13, true), mail_inside: p[15] === 1,
      };
    case PROTO_TYPE.KNOCK:
      if (p.length !== 4) return null;
      return { type: "knock", count: dv.getUint16(0, true), mag: dv.getUint16(2, true) / 1000 };
    case PROTO_TYPE.REPLY:
      if (p.length < 1) return null;
      return { type: "reply", ok: p[0] === 1, message: new TextDecoder().decode(p.subarray(1)) };
    case PROTO_TYPE.STATUS: {
      if (p.length < 1) return null;
      const s = { type: "status", comp: p[0] };
      const f = p.subarray(1);
      const fv = new DataView(f.buffer, f.byteOffset, f.byteLength);
      if (s.comp === 1 && f.length === 7) {
        Object.assign(s, { name: "power", v_rest: fv.getUint16(0, true), v_sag: fv.getUint16(2, true),
          v_live: fv.getUint16(4, true), temp: fv.getInt8(6) });
      } else if (s.comp === 2 && f.length === 13) {
        Object.assign(s, { name: "imu", present: f[0] === 1, pitch: fv.getInt16(1, true) / 10,
          ax: fv.getInt16(3, true) / 1000, ay: fv.getInt16(5, true) / 1000, az: fv.getInt16(7, true) / 1000,
          mag: fv.getUint16(9, true) / 1000, zero: fv.getInt16(11, true) / 10 });
      } else if (s.comp === 3 && f.length >= 6) {
        const n = f[4];
        if (f.length < 6 + n) return null;
        const m = f[5 + n];
        if (f.length !== 6 + n + m) return null;
        const a = (off, len) => String.fromCharCode(...f.subarray(off, off + len));
        Object.assign(s, { name: "lora", fcnt: fv.getUint32(0, true), session: a(5, n), last_payload: a(6 + n, m) });
      } else if (s.comp === 4 && f.length === 2) {
        Object.assign(s, { name: "knock", count: fv.getUint16(0, true) });
      } else if (s.comp === 5 && f.length >= 6 && f.length === 6 + f[5]) {
        Object.assign(s, { name: "system", bench: f[0] === 1, usb: f[1] === 1, mail_inside: f[2] === 1,
          stream_ms: fv.getUint16(3, true), fw: String.fromCharCode(...f.subarray(6, 6 + f[5])) });
      } else return null;
      return s;
    }
    default:
      return null;
  }
}

// Streaming decoder: push(Uint8Array) in arbitrary chunks; onFrame(obj) per valid frame.
// Resyncs by dropping one byte whenever sync, length or crc is bad. Unknown/malformed frames are skipped.
function createProtoDecoder(onFrame) {
  let buf = [];
  return function push(chunk) {
    for (const b of chunk) buf.push(b);
    for (;;) {
      while (buf.length && buf[0] !== PROTO_SYNC) buf.shift();
      if (buf.length < 3) return;
      const len = buf[2];
      if (len > PROTO_MAX_PAYLOAD) { buf.shift(); continue; }
      if (buf.length < len + 4) return;
      const body = buf.slice(1, 3 + len);
      if (protoCrc8(body) !== buf[3 + len]) { buf.shift(); continue; }
      const obj = protoDecodePayload(buf[1], Uint8Array.from(buf.slice(3, 3 + len)));
      buf = buf.slice(len + 4);
      if (obj) onFrame(obj);
    }
  };
}

if (typeof module !== "undefined") module.exports = { protoCrc8, protoDecodePayload, createProtoDecoder, PROTO_TYPE };
