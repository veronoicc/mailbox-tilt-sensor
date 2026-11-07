#pragma once
// Copy this file to include/secrets.h (gitignored) and fill in your values.
// Hex strings go in straight from the TTN console: spaces, dashes, colons and 0x prefixes are ignored.

#define TTN_JOIN_EUI_HEX  "0000000000000000"
#define TTN_DEV_EUI_HEX   "70B3D57ED0060000"
#define TTN_APP_KEY_HEX   "00112233445566778899AABBCCDDEEFF"
// LoRaWAN 1.1 only; 1.0.x uses the AppKey for both.
// #define TTN_NWK_KEY_HEX "00112233445566778899AABBCCDDEEFF"

// PIN required by the Web Bluetooth console.
#define BLE_AUTH_PIN      "123456"
