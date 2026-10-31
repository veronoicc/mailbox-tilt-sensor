function decodeUplink(input) {
  var bytes = input.bytes;
  var fPort = input.fPort;

  // FPort 1: mail activity and knock events (7 bytes)
  if (fPort === 1) {
    if (bytes.length < 7) {
      return { errors: ["Invalid FPort 1 payload length: expected 7 bytes, got " + bytes.length] };
    }

    var battery_mv = 2000 + (bytes[0] * 10);
    var battery_v  = battery_mv / 1000.0;
    var battery_sag_mv = bytes[1] * 5;
    var peak_tilt_deg = bytes[2];
    var resting_tilt_deg = bytes[3];
    var open_duration_s = bytes[4] * 0.1;
    var temperature_c = (bytes[5] > 127) ? bytes[5] - 256 : bytes[5];
    var event_code = bytes[6];
    var event_name = (event_code === 0x02) ? "mailbox_emptied" : "mail_delivered";
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
        event: event_name,
        event_code: event_code,
        battery_voltage: battery_v,
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

  // FPort 2: periodic keepalive (5 bytes)
  if (fPort === 2) {
    if (bytes.length < 5) {
      return { errors: ["Invalid FPort 2 payload length: expected 5 bytes, got " + bytes.length] };
    }

    var hb_battery_mv = 2000 + (bytes[0] * 10);
    var hb_battery_v  = hb_battery_mv / 1000.0;
    var hb_sag_mv     = bytes[1] * 5;
    var hb_rest_tilt  = bytes[2];
    var hb_temp_c     = (bytes[3] > 127) ? bytes[3] - 256 : bytes[3];
    var hb_counter    = bytes[4];

    return {
      data: {
        event: "heartbeat",
        battery_voltage: hb_battery_v,
        battery_mv: hb_battery_mv,
        battery_sag_mv: hb_sag_mv,
        resting_tilt_deg: hb_rest_tilt,
        temperature_c: hb_temp_c,
        heartbeat_counter: hb_counter,
        is_jammed: (hb_rest_tilt > 10)
      }
    };
  }

  return { errors: ["Unsupported FPort: " + fPort] };
}
