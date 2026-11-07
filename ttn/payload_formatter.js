// Paste into TTN Console -> Payload formatters (Custom Javascript formatter), uplink and downlink.
// Layout must match src/lora/uplink.cpp.
function decodeUplink(input) {
  var bytes = input.bytes;
  var TYPES = { 1: "mail_inserted", 2: "mail_cleared", 3: "heartbeat", 4: "manual_test" };
  var LENGTHS = { 1: 7, 2: 5, 3: 6, 4: 7 };
  var SOURCES = ["knock", "downlink", "console"];
  var REASONS = ["timer", "startup", "requested", "poll"];

  if (input.fPort !== 1) {
    return { errors: ["Unsupported FPort: " + input.fPort] };
  }
  if (bytes.length < 1) {
    return { errors: ["Empty payload"] };
  }
  var code = bytes[0];
  if (!TYPES[code]) {
    return { errors: ["Unknown message type: " + code] };
  }
  if (bytes.length !== LENGTHS[code]) {
    return { errors: ["Invalid " + TYPES[code] + " payload length: expected " + LENGTHS[code] + " bytes, got " + bytes.length] };
  }

  var battery_mv = 2000 + (bytes[1] * 10);
  var data = {
    type: TYPES[code],
    battery_voltage: battery_mv / 1000.0,
    battery_mv: battery_mv,
    battery_sag_mv: bytes[2] * 5,
    temperature_c: (bytes[3] > 127) ? bytes[3] - 256 : bytes[3]
  };

  if (code === 1 || code === 4) {
    data.peak_tilt_deg = bytes[4];
    data.resting_tilt_deg = bytes[5];
    data.open_duration_s = Math.round(bytes[6] * 0.1 * 10) / 10;
  }
  if (code === 1) {
    data.is_jammed = bytes[5] > 10;
    data.mail_volume = "none";
    if (bytes[4] > 55) {
      data.mail_volume = "parcel_or_thick_catalog";
    } else if (bytes[4] > 25) {
      data.mail_volume = "large_envelope";
    } else if (bytes[4] >= 12) {
      data.mail_volume = "letter";
    }
  } else if (code === 2) {
    if (!SOURCES[bytes[4]]) {
      return { errors: ["Unknown clear source: " + bytes[4]] };
    }
    data.source = SOURCES[bytes[4]];
  } else if (code === 3) {
    if (!REASONS[bytes[5]]) {
      return { errors: ["Unknown heartbeat reason: " + bytes[5]] };
    }
    data.mail_inside = bytes[4] !== 0;
    data.reason = REASONS[bytes[5]];
  }
  return { data: data };
}

var DOWNLINK_ACTIONS = { heartbeat: 1, clear: 2 };

function encodeDownlink(input) {
  var action = input.data && input.data.action;
  if (!DOWNLINK_ACTIONS.hasOwnProperty(action)) {
    return { errors: ["Unknown action: " + action] };
  }
  return { bytes: [DOWNLINK_ACTIONS[action]], fPort: 2 };
}

function decodeDownlink(input) {
  if (input.fPort !== 2) {
    return { errors: ["Unsupported FPort: " + input.fPort] };
  }
  if (input.bytes.length < 1) {
    return { errors: ["Empty payload"] };
  }
  for (var action in DOWNLINK_ACTIONS) {
    if (DOWNLINK_ACTIONS[action] === input.bytes[0]) {
      return { data: { action: action } };
    }
  }
  return { errors: ["Unknown opcode: " + input.bytes[0]] };
}
