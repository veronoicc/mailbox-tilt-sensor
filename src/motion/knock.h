#pragma once
#include <stdint.h>
// Knock detection on top of the accelerometer magnitude.

// True (and lastHitMs updated) if magG is above the threshold and the debounce has elapsed.
bool knockHit(float magG, uint32_t nowMs, uint32_t& lastHitMs);
// Blocks up to KNOCK_WINDOW_MS; true if KNOCK_COUNT_REQUIRED hits arrive. The wake-up
// vibration counts as hit #1.
bool knockPatternDetected();
