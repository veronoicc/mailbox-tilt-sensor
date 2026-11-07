"use strict";
const test = require("node:test");
const assert = require("node:assert");
const { protoCrc8, createProtoDecoder } = require("./proto.js");

// Vectors (mirrored in test/test_proto on the firmware side), hex:
// TELEMETRY     a50110860b1783ffdeff0c00e603e80303000129  v_live=2950 temp=23 pitch=-125 ax=-34 ay=12 az=998 mag=1000 knocks=3 mail_inside=1
// KNOCK         a5020402003a079f                        count=2 mag=1850
// REPLY         a50303014f4b99                          ok=1 "OK"
// STATUS_POWER  a5040801b80b7800860bfbda                rest=3000 sag=120 live=2950 temp=-5
// STATUS_IMU    a5040e02012000deff0c00e603e8030f0022    present=1 pitch=32 ax=-34 ay=12 az=998 mag=1000 zero=15
// STATUS_LORA   a504110370110100064a4f494e4544043041314283  fcnt=70000 "JOINED" "0A1B"
// STATUS_KNOCK  a5040304030021                          count=3
// STATUS_SYSTEM a5040c05010001640005312e322e33ac  bench=1 usb=0 mail_inside=1 stream=100 "1.2.3"
const V = {
  telemetry: "a50110860b1783ffdeff0c00e603e80303000129",
  knock: "a5020402003a079f",
  reply: "a50303014f4b99",
  power: "a5040801b80b7800860bfbda",
  imu: "a5040e02012000deff0c00e603e8030f0022",
  lora: "a504110370110100064a4f494e4544043041314283",
  knockStatus: "a5040304030021",
  system: "a5040c05010001640005312e322e33ac",
};
const bytes = (h) => Uint8Array.from(Buffer.from(h, "hex"));

function decodeAll(chunks) {
  const out = [];
  const push = createProtoDecoder((f) => out.push(f));
  chunks.forEach(push);
  return out;
}
const one = (h) => decodeAll([bytes(h)])[0];

test("crc8 poly 0x07", () => {
  assert.strictEqual(protoCrc8(bytes("313233343536373839")), 0xf4); // standard CRC-8 check value
});

test("telemetry", () => {
  assert.deepStrictEqual(one(V.telemetry), { type: "telemetry", v_live: 2950, temp: 23, pitch: -12.5,
    ax: -0.034, ay: 0.012, az: 0.998, mag: 1, knocks: 3, mail_inside: true });
});
test("knock", () => assert.deepStrictEqual(one(V.knock), { type: "knock", count: 2, mag: 1.85 }));
test("reply", () => assert.deepStrictEqual(one(V.reply), { type: "reply", ok: true, message: "OK" }));
test("status power", () => assert.deepStrictEqual(one(V.power),
  { type: "status", comp: 1, name: "power", v_rest: 3000, v_sag: 120, v_live: 2950, temp: -5 }));
test("status imu", () => assert.deepStrictEqual(one(V.imu),
  { type: "status", comp: 2, name: "imu", present: true, pitch: 3.2, ax: -0.034, ay: 0.012, az: 0.998, mag: 1, zero: 1.5 }));
test("status lora", () => assert.deepStrictEqual(one(V.lora),
  { type: "status", comp: 3, name: "lora", fcnt: 70000, session: "JOINED", last_payload: "0A1B" }));
test("status knock", () => assert.deepStrictEqual(one(V.knockStatus), { type: "status", comp: 4, name: "knock", count: 3 }));
test("status system", () => assert.deepStrictEqual(one(V.system),
  { type: "status", comp: 5, name: "system", bench: true, usb: false, mail_inside: true, stream_ms: 100, fw: "1.2.3" }));

test("byte-by-byte chunking reassembles all frames", () => {
  const all = Object.values(V).join("");
  const chunks = [...bytes(all)].map((b) => Uint8Array.of(b));
  assert.strictEqual(decodeAll(chunks).length, Object.keys(V).length);
});

test("bad crc and garbage resync", () => {
  const bad = V.knock.slice(0, -2) + "00"; // corrupt crc
  const stream = "00ff" + bad + "a5" + V.reply + V.knock; // garbage, bad frame, stray sync, good frames
  const out = decodeAll([bytes(stream.slice(0, 10)), bytes(stream.slice(10))]);
  assert.deepStrictEqual(out.map((f) => f.type), ["reply", "knock"]);
});

test("oversize length resyncs", () => {
  const out = decodeAll([bytes("a501ff" + V.knock)]);
  assert.deepStrictEqual(out.map((f) => f.type), ["knock"]);
});
