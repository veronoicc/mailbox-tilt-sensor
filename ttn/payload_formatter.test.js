// Byte vectors mirror test/test_uplink — keep both sides in sync.
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const ctx = {};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, "payload_formatter.js"), "utf8"), ctx);

// Round-trip through JSON: objects from another VM realm have foreign prototypes.
const j = (v) => JSON.parse(JSON.stringify(v));
const decode = (bytes, fPort = 1) => j(ctx.decodeUplink({ bytes, fPort }));

test("mail_inserted", () => {
  assert.deepEqual(decode([0x01, 101, 9, 18, 48, 0, 8]).data, {
    type: "mail_inserted",
    battery_voltage: 3.01,
    battery_mv: 3010,
    battery_sag_mv: 45,
    temperature_c: 18,
    peak_tilt_deg: 48,
    resting_tilt_deg: 0,
    open_duration_s: 0.8,
    is_jammed: false,
    mail_volume: "large_envelope",
  });
});

test("clamped values", () => {
  const d = decode([0x01, 0, 255, 0xf4, 90, 0, 255]).data;
  assert.equal(d.battery_mv, 2000);
  assert.equal(d.battery_sag_mv, 1275);
  assert.equal(d.temperature_c, -12);
  assert.equal(d.peak_tilt_deg, 90);
  assert.equal(d.open_duration_s, 25.5);
  assert.equal(d.mail_volume, "parcel_or_thick_catalog");
});

test("manual_test", () => {
  assert.deepEqual(decode([0x04, 255, 0, 0, 13, 90, 0]).data, {
    type: "manual_test",
    battery_voltage: 4.55,
    battery_mv: 4550,
    battery_sag_mv: 0,
    temperature_c: 0,
    peak_tilt_deg: 13,
    resting_tilt_deg: 90,
    open_duration_s: 0,
  });
});

test("jammed flap", () => {
  assert.equal(decode([0x01, 101, 0, 18, 40, 15, 20]).data.is_jammed, true);
});

test("mail_cleared", () => {
  assert.deepEqual(decode([0x02, 101, 0, 18, 2]).data, {
    type: "mail_cleared",
    battery_voltage: 3.01,
    battery_mv: 3010,
    battery_sag_mv: 0,
    temperature_c: 18,
    source: "console",
  });
  assert.equal(decode([0x02, 101, 0, 18, 0]).data.source, "knock");
  assert.equal(decode([0x02, 101, 0, 18, 1]).data.source, "downlink");
});

test("heartbeat", () => {
  assert.deepEqual(decode([0x03, 101, 9, 18, 1, 1]).data, {
    type: "heartbeat",
    battery_voltage: 3.01,
    battery_mv: 3010,
    battery_sag_mv: 45,
    temperature_c: 18,
    mail_inside: true,
    reason: "startup",
  });
  const d = decode([0x03, 101, 9, 18, 0, 3]).data;
  assert.equal(d.mail_inside, false);
  assert.equal(d.reason, "poll");
});

test("unsupported fPort", () => {
  assert.deepEqual(decode([1, 2, 3, 4, 5], 2), { errors: ["Unsupported FPort: 2"] });
});

test("empty payload", () => {
  assert.equal(decode([]).errors.length, 1);
});

test("unknown type", () => {
  assert.equal(decode([0x09, 101, 0, 18, 0]).errors.length, 1);
});

test("wrong length", () => {
  assert.equal(decode([0x01, 101, 9, 18, 48, 0]).errors.length, 1);
  assert.equal(decode([0x02, 101, 0, 18]).errors.length, 1);
  assert.equal(decode([0x03, 101, 0, 18, 1, 1, 0]).errors.length, 1);
});

test("encodeDownlink", () => {
  assert.deepEqual(j(ctx.encodeDownlink({ data: { action: "heartbeat" } })), { bytes: [1], fPort: 2 });
  assert.deepEqual(j(ctx.encodeDownlink({ data: { action: "clear" } })), { bytes: [2], fPort: 2 });
  assert.equal(j(ctx.encodeDownlink({ data: { action: "nope" } })).errors.length, 1);
  assert.equal(j(ctx.encodeDownlink({ data: {} })).errors.length, 1);
});

test("decodeDownlink", () => {
  assert.deepEqual(j(ctx.decodeDownlink({ bytes: [1], fPort: 2 })), { data: { action: "heartbeat" } });
  assert.deepEqual(j(ctx.decodeDownlink({ bytes: [2], fPort: 2 })), { data: { action: "clear" } });
  assert.equal(j(ctx.decodeDownlink({ bytes: [9], fPort: 2 })).errors.length, 1);
  assert.equal(j(ctx.decodeDownlink({ bytes: [], fPort: 2 })).errors.length, 1);
  assert.equal(j(ctx.decodeDownlink({ bytes: [1], fPort: 1 })).errors.length, 1);
});
