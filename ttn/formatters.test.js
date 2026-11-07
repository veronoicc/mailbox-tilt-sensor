// Byte vectors mirror test/test_uplink — keep both sides in sync.
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

// One context per file: each is pasted into its own TTN tab and must stand alone.
const load = (f) => {
  const c = {};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, f), "utf8"), c);
  return c;
};
const upCtx = load("uplink_formatter.js");
const downCtx = load("downlink_formatter.js");

// Round-trip through JSON: objects from another VM realm have foreign prototypes.
const j = (v) => JSON.parse(JSON.stringify(v));
const decode = (bytes, fPort = 1) => j(upCtx.decodeUplink({ bytes, fPort }));

test("mail_inserted", () => {
  assert.deepEqual(decode([0x01, 101, 9, 18, 1, 48, 0, 8]).data, {
    reason: "mail_inserted",
    battery: 3010,
    battery_sag: 45,
    temperature: 18,
    mail_inside: true,
    peak_tilt: 48,
    resting_tilt: 0,
    open_duration: 800,
    is_jammed: false,
    mail_volume: "large_envelope",
  });
});

test("clamped values", () => {
  const d = decode([0x01, 0, 255, 0xf4, 0, 90, 0, 255]).data;
  assert.equal(d.battery, 2000);
  assert.equal(d.battery_sag, 1275);
  assert.equal(d.temperature, -12);
  assert.equal(d.mail_inside, false);
  assert.equal(d.peak_tilt, 90);
  assert.equal(d.open_duration, 25500);
  assert.equal(d.mail_volume, "parcel_or_thick_catalog");
});

test("manual_test", () => {
  assert.deepEqual(decode([0x07, 255, 0, 0, 0, 13, 90, 0]).data, {
    reason: "manual_test",
    battery: 4550,
    battery_sag: 0,
    temperature: 0,
    mail_inside: false,
    peak_tilt: 13,
    resting_tilt: 90,
    open_duration: 0,
  });
});

test("jammed flap", () => {
  assert.equal(decode([0x01, 101, 0, 18, 1, 40, 15, 20]).data.is_jammed, true);
});

test("every reason carries the full frame", () => {
  const names = {
    2: "mail_cleared",
    3: "heartbeat",
    4: "heartbeat_requested",
    5: "heartbeat_startup",
    6: "heartbeat_poll",
  };
  for (const code of Object.keys(names)) {
    assert.deepEqual(decode([Number(code), 101, 9, 18, 1, 3, 3, 0]).data, {
      reason: names[code],
      battery: 3010,
      battery_sag: 45,
      temperature: 18,
      mail_inside: true,
      peak_tilt: 3,
      resting_tilt: 3,
      open_duration: 0,
    });
  }
});

test("unsupported fPort", () => {
  assert.deepEqual(decode([1, 2, 3, 4, 5, 6, 7, 8], 2), { errors: ["Unsupported FPort: 2"] });
});

test("empty payload", () => {
  assert.equal(decode([]).errors.length, 1);
});

test("unknown reason", () => {
  assert.equal(decode([0x09, 101, 0, 18, 0, 0, 0, 0]).errors.length, 1);
});

test("wrong length", () => {
  assert.equal(decode([0x03, 101, 9, 18, 1, 1]).errors.length, 1);
});

test("encodeDownlink", () => {
  assert.deepEqual(j(downCtx.encodeDownlink({ data: { action: "heartbeat" } })), { bytes: [1], fPort: 2 });
  assert.deepEqual(j(downCtx.encodeDownlink({ data: { action: "clear" } })), { bytes: [2], fPort: 2 });
  assert.equal(j(downCtx.encodeDownlink({ data: { action: "nope" } })).errors.length, 1);
});

test("decodeDownlink", () => {
  assert.deepEqual(j(downCtx.decodeDownlink({ bytes: [1], fPort: 2 })), { data: { action: "heartbeat" } });
  assert.deepEqual(j(downCtx.decodeDownlink({ bytes: [2], fPort: 2 })), { data: { action: "clear" } });
  assert.equal(j(downCtx.decodeDownlink({ bytes: [9], fPort: 2 })).errors.length, 1);
  assert.equal(j(downCtx.decodeDownlink({ bytes: [1], fPort: 1 })).errors.length, 1);
});
