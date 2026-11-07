#pragma once
#include <Arduino.h>

// Seeed XIAO nRF52840 + Wio-SX1262 kit + Grove LIS3DHTR
#define PIN_LORA_NSS    D4
#define PIN_LORA_NRST   D2
#define PIN_LORA_BUSY   D3
#define PIN_LORA_DIO1   D1
#define PIN_LORA_RXEN   D5    // Wio-SX1262 antenna-switch RX enable (DIO2 drives TX)
#define LORA_TCXO_VOLTAGE 1.8f  // Wio-SX1262 TCXO supply from DIO3

#define PIN_ACCEL_INT   D0    // LIS3DH INT1, the System-Off wake source
#define LIS3DH_I2C_ADDR 0x19  // Grove default address (SDO pulled high)

// Optional: uncomment to override the default I2C pins (XIAO D4 = SDA, D5 = SCL).
// Both must be set together.
#define PIN_ACCEL_SDA   D7
#define PIN_ACCEL_SCL   D6
#if defined(PIN_ACCEL_SDA) != defined(PIN_ACCEL_SCL)
#error "Define both PIN_ACCEL_SDA and PIN_ACCEL_SCL"
#endif

// The XIAO user LEDs are active-low.
#define XIAO_LED_ON     LOW
#define XIAO_LED_OFF    HIGH
