#pragma once
#include <Arduino.h>

// ==========================================
// HARDWARE PINOUT (Seeed XIAO nRF52840 + Wio-SX1262)
// Do not modify unless using custom wiring.
// ==========================================
#ifndef PIN_LORA_NSS
#define PIN_LORA_NSS    D1
#endif
#ifndef PIN_LORA_NRST
#define PIN_LORA_NRST   D2
#endif
#ifndef PIN_LORA_BUSY
#define PIN_LORA_BUSY   D3
#endif
#ifndef PIN_LORA_DIO1
#define PIN_LORA_DIO1   D6
#endif

// Tilt Sensor I2C & Interrupt Pin
#define PIN_ACCEL_INT   D0    // INT1 pin on LIS3DHTR
#define LIS3DH_I2C_ADDR 0x19  // 0x19: Grove default (SDO pulled high)
