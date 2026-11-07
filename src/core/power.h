#pragma once
#include <stdint.h>
// Power, reset, watchdog, battery and temperature helpers for the nRF52840.

bool usbConnected();
// System-Off would strand the device (no IMU to wake it) or is unwanted on the bench (USB).
bool benchMode();
// True if this boot is a wake from System-Off. Clears and logs the reset reason.
bool wokeFromSystemOff();
void dcdcEnable();

// No-ops when ENABLE_WATCHDOG is off.
void watchdogStart();
void watchdogFeed();

uint16_t batteryMv();
void batterySampleRest();       // remember the resting voltage (before TX)
uint16_t batteryRestMv();       // default 3000 until sampled
uint16_t batterySampleSag();    // measure under/after load, store and return rest - load
uint16_t batterySagMv();        // last stored sag, default 0

int8_t temperatureC();
