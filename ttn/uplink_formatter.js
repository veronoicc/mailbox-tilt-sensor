// Paste into TTN Console -> Payload formatters -> Uplink (Custom Javascript formatter).
// Layout must match src/lora/uplink.cpp.
function decodeUplink(input) {
  var bytes = input.bytes;
  var REASONS = {
    1: "mail_inserted",
    2: "mail_cleared",
    3: "heartbeat",
    4: "heartbeat_requested",
    5: "heartbeat_startup",
    6: "heartbeat_poll",
    7: "manual_test"
  };
  var LENGTH = 8;

  if (input.fPort !== 1) {
    return { errors: ["Unsupported FPort: " + input.fPort] };
  }
  if (bytes.length < 1) {
    return { errors: ["Empty payload"] };
  }
  var code = bytes[0];
  if (!REASONS[code]) {
    return { errors: ["Unknown reason: " + code] };
  }
  if (bytes.length !== LENGTH) {
    return { errors: ["Invalid payload length: expected " + LENGTH + " bytes, got " + bytes.length] };
  }

  var battery_mv = 2000 + (bytes[1] * 10);
  var data = {
    reason: REASONS[code],
    battery: battery_mv,
    battery_sag: bytes[2] * 5,
    temperature: (bytes[3] > 127) ? bytes[3] - 256 : bytes[3],
    mail_inside: bytes[4] !== 0,
    peak_tilt: bytes[5],
    resting_tilt: bytes[6],
    open_duration: bytes[7] * 100
  };

  if (code === 1) {
    data.is_jammed = bytes[6] > 10;
    data.mail_volume = "none";
    if (bytes[5] > 55) {
      data.mail_volume = "parcel_or_thick_catalog";
    } else if (bytes[5] > 25) {
      data.mail_volume = "large_envelope";
    } else if (bytes[5] >= 12) {
      data.mail_volume = "letter";
    }
  }
  return { data: data };
}
