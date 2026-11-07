#pragma once
#include <stdint.h>
// Flap motion profiling after an accelerometer wake.

struct FlapMotion {
  uint32_t startMs;
  float peakDeg;
  float lastDeg;     // last sample inside the loop (initial angle if none)
  float settledDeg;  // one more reading taken after the loop
  uint32_t openMs;   // time spent above FLAP_OPEN_DEG
};

// Samples the flap every 100 ms for up to FLAP_SAMPLE_WINDOW_MS, exiting early once it has
// peaked past FLAP_TRIGGER_DEG and settled.
FlapMotion flapSample(float initialDeg);
// True if the flap is still held past BLE_HOLD_ANGLE_DEG until BLE_HOLD_SECONDS after the start.
bool flapHeldOpen(const FlapMotion& m);
