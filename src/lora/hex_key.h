#pragma once
#include <stddef.h>
#include <stdint.h>
// Parses credential strings pasted from the TTN console. Non-hex characters are skipped,
// as is a "0x"/"0X" that starts a token. Writes at most maxBytes bytes and returns the
// count; a trailing odd nibble is dropped.
size_t hexToBytes(const char* hex, uint8_t* out, size_t maxBytes);
// Big-endian value of the first (up to) 8 bytes, e.g. a JoinEUI/DevEUI.
uint64_t hexToU64(const char* hex);
