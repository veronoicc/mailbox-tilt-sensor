#pragma once
#include <stdint.h>
// Consecutive-startup counter in .noinit RAM (lost on power loss, which counts as 0).
// Increments when realStartup and returns the new value; returns 0 and leaves it alone for a flap wake.
uint8_t startupCounterRecord(bool realStartup);
// Call right before a clean sleep.
void startupCounterClear();
