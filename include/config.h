#pragma once
#include <Arduino.h>

// Seeed XIAO nRF52840 + Wio-SX1262 pinout
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

// LIS3DHTR I2C and interrupt pin
#define PIN_ACCEL_INT   D0
#define LIS3DH_I2C_ADDR 0x19 // Grove default address (SDO pulled high)
