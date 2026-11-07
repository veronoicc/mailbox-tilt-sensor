// Byte vectors mirror test/test_uplink — keep both sides in sync.
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const ctx = {};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, "payload_formatter.js"), "utf8"), ctx);

// Round-trip through JSON: objects from another VM realm have foreign prototypes.
const decode = (bytes, fPort = 1) => JSON.parse(JSON.stringify(ctx.decodeUplink({ bytes, fPort })));

test("mail_delivered", () => {
  assert.deepEqual(decode([101, 9, 48, 0, 8, 18, 1]).data, {
    event: "mail_delivered",
    event_code: 1,
    battery_voltage: 3.01,
    battery_mv: 3010,
    battery_sag_mv: 45,
    peak_tilt_deg: 48,
    resting_tilt_deg: 0,
    open_duration_s: 0.8,
    temperature_c: 18,
    is_jammed: false,
    mail_volume: "large_envelope",
  });
});

test("mailbox_emptied", () => {
  const d = decode([0, 255, 90, 0, 255, 244, 2]).data;
  assert.equal(d.event, "mailbox_emptied");
  assert.equal(d.temperature_c, -12);
  assert.equal(d.battery_mv, 2000);
  assert.equal(d.battery_sag_mv, 1275);
  assert.equal(d.mail_volume, "none");
});

test("manual_test", () => {
  const d = decode([101, 0, 30, 0, 0, 20, 3]).data;
  assert.equal(d.event, "manual_test");
  assert.equal(d.event_code, 3);
  assert.equal(d.mail_volume, "none");
  assert.equal(d.is_jammed, false);
});

test("jammed flap", () => {
  assert.equal(decode([101, 0, 40, 15, 20, 18, 1]).data.is_jammed, true);
});

test("unknown event", () => {
  assert.equal(decode([101, 0, 40, 0, 0, 18, 9]).data.event, "unknown");
});

test("unsupported fPort", () => {
  assert.deepEqual(decode([1, 2, 3, 4, 5], 2), { errors: ["Unsupported FPort: 2"] });
});

test("short payload", () => {
  assert.deepEqual(decode([1, 2, 3, 4, 5, 6]), {
    errors: ["Invalid FPort 1 payload length: expected 7 bytes, got 6"],
  });
});
