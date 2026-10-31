#pragma once
#include <RadioLib.h>

// Options: EU868, US915, AU915, AS923, KR920, IN865
#define LORAWAN_BAND               EU868

// The Things Network OTAA credentials.
// Plain hex strings copied from the TTN console. Delimiters and prefixes are ignored.
#define TTN_JOIN_EUI_HEX           "0000000000000000"
#define TTN_DEV_EUI_HEX            "70B3D57ED0060000"
#define TTN_APP_KEY_HEX            "00112233445566778899AABBCCDDEEFF"
// #define TTN_NWK_KEY_HEX         "00112233445566778899AABBCCDDEEFF"

// Feature toggles
#define ENABLE_HEARTBEAT           true
#define HEARTBEAT_INTERVAL_HOURS   24

#define ENABLE_TEMPERATURE         true
#define ENABLE_BATTERY_SAG         true

// Knock detection: physical knocks to trigger a "mailbox emptied" event.
#define ENABLE_KNOCK_DETECTION     true
#define KNOCK_COUNT_REQUIRED       4
#define KNOCK_WINDOW_MS            2200
#define KNOCK_ABORT_TIMEOUT_MS     500

// WebBluetooth maintenance: hold flap open past 60 degrees for 15s to advertise.
#define ENABLE_BLE_MAINTENANCE     true
#define BLE_DEVICE_NAME            "Mailbox-Sensor"
#define BLE_HOLD_OPEN_SECONDS      15
#define BLE_TIMEOUT_SECONDS        60
#define BLE_AUTH_PIN               "123456"

// Accelerometer sample rate: 10 Hz (~6 µA, 100 ms reaction) or 1 Hz (~2 µA).
#define SENSOR_1HZ_ULTRA_LOW_POWER false

// Thresholds in degrees
#define TILT_TRIGGER_THRESHOLD_DEG 12.0f
#define JAMMED_THRESHOLD_DEG       10.0f
#define BLE_TRIGGER_ANGLE_DEG      60.0f
#define MAX_SAMPLE_WINDOW_MS       3000
#define DEBOUNCE_COOLDOWN_SEC      30
