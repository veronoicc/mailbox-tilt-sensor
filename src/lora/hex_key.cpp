// Tolerant hex parser for TTN credential strings.
#include "lora/hex_key.h"

static int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

size_t hexToBytes(const char* hex, uint8_t* out, size_t maxBytes) {
  size_t n = 0;
  int high = -1;
  bool prevWasHex = false;
  for (const char* p = hex; *p && n < maxBytes; p++) {
    if (*p == '0' && !prevWasHex && (p[1] == 'x' || p[1] == 'X')) {
      p++;
      continue;
    }
    int v = nibble(*p);
    prevWasHex = v >= 0;
    if (v < 0) continue;
    if (high < 0) {
      high = v;
    } else {
      out[n++] = (uint8_t)(high << 4 | v);
      high = -1;
    }
  }
  return n;
}

uint64_t hexToU64(const char* hex) {
  uint8_t b[8];
  size_t n = hexToBytes(hex, b, 8);
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) v = v << 8 | b[i];
  return v;
}
