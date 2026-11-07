// LIS3DH accelerometer driver.
#include "motion/imu.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "board.h"
#include "settings.h"
#include "core/log.h"

static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t WHO_AM_I_LIS3DH = 0x33;
static constexpr uint8_t REG_CTRL1 = 0x20;
static constexpr uint8_t REG_CTRL2 = 0x21;
static constexpr uint8_t REG_CTRL3 = 0x22;
static constexpr uint8_t REG_CTRL4 = 0x23;
static constexpr uint8_t REG_REFERENCE = 0x26;
static constexpr uint8_t REG_OUT_X_L = 0x28;
static constexpr uint8_t REG_INT1_CFG = 0x30;
static constexpr uint8_t REG_INT1_SRC = 0x31;
static constexpr uint8_t REG_INT1_THS = 0x32;
static constexpr uint8_t REG_INT1_DURATION = 0x33;
static constexpr uint8_t AUTO_INCREMENT = 0x80;

static bool g_present = false;
static float g_tiltZero = 0.0f;

static void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)1);
  return Wire.read();
}

static float rawPitchDeg(float x, float y, float z) {
  return fabsf(atan2f(x, sqrtf(y * y + z * z)) * RAD_TO_DEG);
}

// Reads x/y/z in g; false if the IMU is absent or I2C fails.
static bool readXYZ(float& x, float& y, float& z) {
  if (!g_present) return false;
  Wire.beginTransmission(LIS3DH_I2C_ADDR);
  Wire.write(REG_OUT_X_L | AUTO_INCREMENT);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)LIS3DH_I2C_ADDR, (uint8_t)6) < 6) return false;

  int16_t rawX = Wire.read() | (Wire.read() << 8);
  int16_t rawY = Wire.read() | (Wire.read() << 8);
  int16_t rawZ = Wire.read() | (Wire.read() << 8);
  x = (float)rawX / 16384.0f;
  y = (float)rawY / 16384.0f;
  z = (float)rawZ / 16384.0f;
  return true;
}

bool imuBegin() {
  Wire.begin();
  delay(15);

  uint8_t id = readReg(REG_WHO_AM_I);
  if (id != WHO_AM_I_LIS3DH) {
    g_present = false;
    DBG_PRINTF("[IMU] LIS3DH not detected at 0x%02X (WHO_AM_I = 0x%02X, expected 0x33).\n", LIS3DH_I2C_ADDR, id);
    DBG_PRINTLN("[IMU] Board is in bench mode without IMU. Plug LIS3DH to D4/D5 when ready.");
    return false;
  }

  g_present = true;
  DBG_PRINTLN("[IMU] LIS3DH detected (WHO_AM_I = 0x33). Initializing registers...");

  // Set 1 Hz (~2 µA) or 10 Hz (~6 µA) low-power sample rate.
  writeReg(REG_CTRL1, IMU_RATE_1HZ ? 0x1F : 0x2F);

  // High-pass filter in reference mode detects motion deltas rather than static tilt.
  writeReg(REG_CTRL2, 0x09);
  writeReg(REG_CTRL3, 0x40);  // Route interrupt 1 to INT1 pin.
  writeReg(REG_CTRL4, 0x00);  // +/-2g full scale.
  writeReg(REG_INT1_THS, 0x0A);       // Motion threshold: ~160 mg (~10-12 deg deflection).
  writeReg(REG_INT1_DURATION, 0x01);  // 1 sample duration filter.
  writeReg(REG_INT1_CFG, 0x2A);       // Enable X/Y/Z high-event interrupt generation.

  // Latch current resting orientation into the reference register.
  readReg(REG_REFERENCE);
  return true;
}

bool imuPresent() {
  return g_present;
}

AccelSample imuRead() {
  AccelSample a = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  if (!readXYZ(a.x, a.y, a.z)) return a;
  a.mag = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
  float pitch = rawPitchDeg(a.x, a.y, a.z) - g_tiltZero;
  a.pitch = (pitch < 0.0f) ? 0.0f : pitch;
  return a;
}

float imuTiltZero() {
  return g_tiltZero;
}

float imuCalibrateZero() {
  float x, y, z;
  g_tiltZero = readXYZ(x, y, z) ? rawPitchDeg(x, y, z) : 0.0f;
  return g_tiltZero;
}

void imuArmWake() {
  readReg(REG_INT1_SRC);
  readReg(REG_REFERENCE);  // Latch resting vector to prevent wake loops if flap remains propped open.
}
