// Paste into TTN Console -> Payload formatters -> Uplink (Custom Javascript formatter).
// Layout must match src/lora/uplink.cpp.
function decodeUplink(input) {
  var bytes = input.bytes;
  var EVENTS = { 1: "mail_delivered", 2: "mailbox_emptied", 3: "manual_test" };

  if (input.fPort !== 1) {
    return { errors: ["Unsupported FPort: " + input.fPort] };
  }
  if (bytes.length < 7) {
    return { errors: ["Invalid FPort 1 payload length: expected 7 bytes, got " + bytes.length] };
  }

  var battery_mv = 2000 + (bytes[0] * 10);
  var battery_sag_mv = bytes[1] * 5;
  var peak_tilt_deg = bytes[2];
  var resting_tilt_deg = bytes[3];
  var open_duration_s = bytes[4] * 0.1;
  var temperature_c = (bytes[5] > 127) ? bytes[5] - 256 : bytes[5];
  var event_code = bytes[6];
  var is_jammed = (event_code === 0x01 && resting_tilt_deg > 10);

  var mail_volume = "none";
  if (event_code === 0x01) {
    if (peak_tilt_deg > 55) {
      mail_volume = "parcel_or_thick_catalog";
    } else if (peak_tilt_deg > 25) {
      mail_volume = "large_envelope";
    } else if (peak_tilt_deg >= 12) {
      mail_volume = "letter";
    }
  }

  return {
    data: {
      event: EVENTS[event_code] || "unknown",
      event_code: event_code,
      battery_voltage: battery_mv / 1000.0,
      battery_mv: battery_mv,
      battery_sag_mv: battery_sag_mv,
      peak_tilt_deg: peak_tilt_deg,
      resting_tilt_deg: resting_tilt_deg,
      open_duration_s: Math.round(open_duration_s * 10) / 10,
      temperature_c: temperature_c,
      is_jammed: is_jammed,
      mail_volume: mail_volume
    }
  };
}
