/**
 * The Things Network (TTN v3) Custom JavaScript Payload Formatter
 * 
 * Paste this into:
 * TTN Console -> Applications -> [Your App] -> Payload Formatters -> Uplink
 * Formatter type: Custom Javascript formatter
 */

function decodeUplink(input) {
  var bytes = input.bytes;
  var fPort = input.fPort;

  // ----------------------------------------------------
  // FPORT 1: ACTIVITY & MAIL EVENTS (7 Bytes)
  // ----------------------------------------------------
  if (fPort === 1) {
    if (bytes.length < 7) {
      return { errors: ["Invalid FPort 1 payload length: expected 7 bytes, got " + bytes.length] };
    }

    // Byte 0: Resting battery voltage (2000mV..4550mV in 10mV steps)
    var battery_mv = 2000 + (bytes[0] * 10);
    var battery_v  = battery_mv / 1000.0;

    // Byte 1: Battery voltage sag under LoRa TX load in 5mV steps
    var battery_sag_mv = bytes[1] * 5;

    // Byte 2: Peak Opening Tilt Angle (0 to 90 degrees)
    var peak_tilt_deg = bytes[2];

    // Byte 3: Resting Tilt Angle after settling (0 to 90 degrees)
    var resting_tilt_deg = bytes[3];

    // Byte 4: Duration Open in 0.1s steps (0 to 25.5s)
    var open_duration_s = bytes[4] * 0.1;

    // Byte 5: Temperature in degrees Celsius (signed int8)
    var temperature_c = (bytes[5] > 127) ? bytes[5] - 256 : bytes[5];

    // Byte 6: Event Code (0x01 = Mail Delivered, 0x02 = Mailbox Emptied)
    var event_code = bytes[6];
    var event_name = "mail_delivered";
    if (event_code === 0x02) {
      event_name = "mailbox_emptied";
    }

    // Anti-Jam Logic
    var is_jammed = (event_code === 0x01 && resting_tilt_deg > 10);

    // Mail Volume Estimation
    var mail_volume = "None";
    if (event_code === 0x01) {
      if (peak_tilt_deg > 55) {
        mail_volume = "Thick Catalog / Parcel";
      } else if (peak_tilt_deg > 25) {
        mail_volume = "Large Magazine / Catalog";
      } else if (peak_tilt_deg >= 12) {
        mail_volume = "Letter / Postcard";
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

  // ----------------------------------------------------
  // FPORT 2: PERIODIC 24-HOUR HEARTBEAT (5 Bytes)
  // ----------------------------------------------------
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

  return {
    errors: ["Unsupported FPort: " + fPort]
  };
}
