#pragma once
#include <Arduino.h>

// Seeed XIAO nRF52840 + Wio-SX1262 kit + Grove LIS3DHTR
#define PIN_LORA_NSS    D1
#define PIN_LORA_NRST   D2
#define PIN_LORA_BUSY   D3
#define PIN_LORA_DIO1   D6

#define PIN_ACCEL_INT   D0    // LIS3DH INT1, the System-Off wake source
#define LIS3DH_I2C_ADDR 0x19  // Grove default address (SDO pulled high)

// The XIAO user LEDs are active-low.
#define XIAO_LED_ON     LOW
#define XIAO_LED_OFF    HIGH
