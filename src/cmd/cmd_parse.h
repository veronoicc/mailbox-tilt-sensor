#pragma once
#include <stdint.h>
// Pure command-line parser (no Arduino includes; builds under the native test env).

constexpr unsigned CMD_LINE_MAX = 128;  // longer input is rejected

struct ParsedLine {
  char pin[16];
  char name[24];       // upper-cased
  char args[4][24];
  uint8_t argc;
  bool hasPin;
};

// Accepts BOTH "PIN:123456 CMD:test_uplink" (BLE/web, args may follow) and the CLI form "test_uplink arg1 arg2".
// Returns false for empty/oversized/malformed input (out is cleared). Never overflows the buffers (input is untrusted).
bool cmdParseLine(const char* line, ParsedLine& out);
// Parses a STREAM interval: decimal digits only. 0 stays 0 (off), anything else is clamped to [minMs, maxMs].
// False (ms untouched) for empty/non-numeric/overlong input.
bool cmdParseStreamMs(const char* s, uint16_t minMs, uint16_t maxMs, uint16_t& ms);
