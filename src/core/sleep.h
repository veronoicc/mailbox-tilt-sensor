#pragma once
#include <stdint.h>
// System-ON sleep: block until the LIS3DH INT1 pin goes high or a timeout elapses.

enum class SleepWake : uint8_t { Motion, Timeout };

// Call imuArmWake() first (it releases the latched INT1 line). Returns Motion immediately if INT1
// is already high. The CPU sleeps in FreeRTOS tickless idle; the RTC keeps the timeout running.
SleepWake sleepWait(uint32_t timeoutMs);
