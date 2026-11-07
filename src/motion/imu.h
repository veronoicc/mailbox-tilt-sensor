#pragma once
#include <stdint.h>
// LIS3DH accelerometer over I2C: setup, readings, tilt calibration and System-Off wake arming.

struct AccelSample {
  float x, y, z;  // g
  float mag;      // g
  float pitch;    // degrees above the calibrated zero, clamped >= 0
};

bool imuBegin();    // returns whether the LIS3DH answered
bool imuPresent();
AccelSample imuRead();  // zeros if the IMU is absent or I2C fails
float imuTiltZero();
float imuCalibrateZero();  // take the current raw pitch as the new zero and return it
void imuArmWake();         // clear the latched interrupt and re-latch the resting reference
