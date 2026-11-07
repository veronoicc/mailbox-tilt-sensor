#pragma once
// User configuration. Credentials live in secrets.h, pins in board.h; serial logging is
// chosen by the PlatformIO env (release/debug), not here.

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "include/secrets.h missing: copy include/secrets.example.h to include/secrets.h and fill in your TTN keys"
#endif

#include <RadioLib.h>

// ── LoRaWAN ──
// Region options: EU868, US915, AU915, AS923, KR920, IN865
#define LORAWAN_BAND               EU868
#define ENABLE_ADR                 true   // let TTN lower data rate / TX power
#define ENABLE_SESSION_PERSISTENCE true   // keep the session in retained RAM across System-Off

// ── System ──
#define ENABLE_WATCHDOG            true   // hardware watchdog resets a frozen CPU
#define WATCHDOG_TIMEOUT_SECONDS   30
#define ENABLE_TEMPERATURE         true   // report CPU temperature (else a fixed 20 C)
#define ENABLE_BATTERY_SAG         true   // measure the voltage drop right after each TX

// ── Flap detection ──
#define FLAP_TRIGGER_DEG           12.0f  // peak tilt needed to count as a delivery
#define FLAP_OPEN_DEG              10.0f  // above this the flap counts as open
#define FLAP_CLOSED_DEG            8.0f   // below this a wake is a knock candidate
#define FLAP_SAMPLE_WINDOW_MS      3000   // max time spent profiling one flap movement
#define IMU_RATE_1HZ               false  // 1 Hz ≈2 µA, 10 Hz ≈6 µA / 100 ms reaction

// ── Knock pattern ──
#define ENABLE_KNOCK_DETECTION     true   // knock pattern on the closed flap = mailbox emptied
#define KNOCK_THRESHOLD_G          1.4f   // acceleration magnitude that counts as a hit
#define KNOCK_DEBOUNCE_MS          160    // min time between hits
#define KNOCK_COUNT_REQUIRED       4
#define KNOCK_WINDOW_MS            2200
#define KNOCK_ABORT_TIMEOUT_MS     500    // give up if no second hit arrives

// ── BLE maintenance ──
#define ENABLE_BLE_MAINTENANCE     true   // Web Bluetooth console
#define BLE_DEVICE_NAME            "Mailbox-Sensor"
#define BLE_BOOT_ADVERTISE_SECONDS 15     // advertise window after power-up
#define BLE_HOLD_ANGLE_DEG         60.0f  // hold the flap past this angle...
#define BLE_HOLD_SECONDS           15     // ...for this long to start BLE
#define BLE_IDLE_TIMEOUT_SECONDS   60     // no client for this long = go to sleep
