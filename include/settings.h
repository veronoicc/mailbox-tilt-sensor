#pragma once
#include <RadioLib.h>

// ==============================================================================
// 1. LORAWAN REGIONAL FREQUENCY BAND
// ==============================================================================
// Options in RadioLib: EU868, US915, AU915, AS923, KR920, IN865
#define LORAWAN_BAND               EU868

// ==============================================================================
// 2. THE THINGS NETWORK (TTN v3) CREDENTIALS
// Simply paste the HEX strings directly from the TTN Console!
// Spaces, colons, dashes, and "0x" prefixes are stripped automatically.
// ==============================================================================

// 16 Hex Characters: JoinEUI / AppEUI (Default in TTN v3 is 0000000000000000)
#define TTN_JOIN_EUI_HEX           "0000000000000000"

// 16 Hex Characters: Device EUI from your TTN device page
#define TTN_DEV_EUI_HEX            "70B3D57ED0060000"

// 32 Hex Characters: AppKey from your TTN device page
#define TTN_APP_KEY_HEX            "00112233445566778899AABBCCDDEEFF"

// Optional NwkKey: In LoRaWAN 1.0.4, NwkKey is identical to AppKey.
// Only uncomment and define this if your network server requires a distinct NwkKey.
// #define TTN_NWK_KEY_HEX         "00112233445566778899AABBCCDDEEFF"

// ==============================================================================
// 3. ADVANCED FEATURE TOGGLES (ALL ENABLED BY DEFAULT)
// Easily disable any feature by setting it to false to save code space / power.
// ==============================================================================

// Feature 1: 24-Hour Periodic Heartbeat / Watchdog Uplink (FPort 2)
#define ENABLE_HEARTBEAT           true
#define HEARTBEAT_INTERVAL_HOURS   24

// Feature 2: Mailbox Temperature Telemetry (reads on-die nRF52 sensor)
#define ENABLE_TEMPERATURE         true

// Feature 3: Battery Voltage Sag Measurement (ESR load test during TX)
#define ENABLE_BATTERY_SAG         true

// Feature 4: "Mailbox Emptied" Reset via 4 Knocks
#define ENABLE_KNOCK_DETECTION     true
#define KNOCK_COUNT_REQUIRED       4      // Number of physical knocks required
#define KNOCK_WINDOW_MS            2200   // Max window for all 4 knocks (ms)
#define KNOCK_ABORT_TIMEOUT_MS     500    // Abort if 2nd knock doesn't arrive within 500ms

// Feature 5: WebBluetooth Maintenance & Configuration Mode
// (Zero power cost when sleeping: BLE radio is completely OFF in deep sleep)
#define ENABLE_BLE_MAINTENANCE     true
#define BLE_DEVICE_NAME            "Mailbox-Sensor"
#define BLE_HOLD_OPEN_SECONDS      15     // Hold flap fully open (>60 deg) for 15s to enter BLE
#define BLE_TIMEOUT_SECONDS        60     // Inactivity timeout before returning to deep sleep
#define BLE_AUTH_PIN               "123456" // PIN required for changing actions (recalibration, DFU)

// ==============================================================================
// 4. POWER SAVING & SENSOR CALIBRATION
// ==============================================================================
// Accelerometer Low-Power Rate:
// false = 10 Hz mode (~6 µA sensor current, 100ms reaction time - Recommended)
// true  = 1 Hz mode  (~2 µA sensor current, ultra-low power floor < 4 µA total)
#define SENSOR_1HZ_ULTRA_LOW_POWER false

#define TILT_TRIGGER_THRESHOLD_DEG 12.0f  // Minimum degrees of flap opening to trigger uplink
#define JAMMED_THRESHOLD_DEG       10.0f  // Resting angle above horizon considered jammed
#define BLE_TRIGGER_ANGLE_DEG      60.0f  // Flap opening angle required for BLE maintenance hold
#define MAX_SAMPLE_WINDOW_MS       3000   // Maximum sample window (exits earlier if flap settles)
#define DEBOUNCE_COOLDOWN_SEC      30     // Cooldown in seconds to prevent duplicate letter triggers
