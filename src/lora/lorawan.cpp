// LoRaWAN OTAA join/restore and uplink on the SX1262.
#include "lora/lorawan.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <string.h>
#include "board.h"
#include "settings.h"
#include "core/log.h"
#include "core/power.h"
#include "lora/hex_key.h"

static SX1262 g_radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);
static LoRaWANNode g_node(&g_radio, &LORAWAN_BAND);

static const char* g_sessionStatus = "COLD_BOOT";
static char g_lastPayloadHex[48] = "None";

#if ENABLE_SESSION_PERSISTENCE
static constexpr uint32_t SESSION_MAGIC = 0xA55A1234;
__attribute__((section(".noinit"))) static uint32_t g_sessionMagic;
__attribute__((section(".noinit"))) static uint8_t g_savedNonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
__attribute__((section(".noinit"))) static uint8_t g_savedSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
#endif

static bool sessionSaved() {
#if ENABLE_SESSION_PERSISTENCE
  return g_sessionMagic == SESSION_MAGIC;
#else
  return false;
#endif
}

// First (up to) 12 bytes as space-separated "%02X".
static void formatHex(const uint8_t* payload, size_t len) {
  char* p = g_lastPayloadHex;
  for (size_t i = 0; i < len && i < 12; i++) {
    p += sprintf(p, "%02X ", payload[i]);
  }
  if (p > g_lastPayloadHex) *(p - 1) = '\0';
}

static bool sendPacket(uint8_t* payload, size_t len) {
  watchdogFeed();
  formatHex(payload, len);

  DBG_PRINTLN("\n----------------------------------------------");
  DBG_PRINTLN("[LoRaWAN TX] Preparing Uplink Transmission");
  DBG_PRINTF("[LoRaWAN TX] JoinEUI: %s | DevEUI: %s\n", TTN_JOIN_EUI_HEX, TTN_DEV_EUI_HEX);
  DBG_PRINTF("[LoRaWAN TX] Retained RAM Session: %s\n",
             sessionSaved() ? "VALID (Restoring Session)" : "COLD_BOOT (Full OTAA Join Needed)");

  int16_t state = g_radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    DBG_PRINTF("[LoRaWAN TX ERROR] Radio begin failed: %d\n", state);
    return false;
  }

  g_radio.setRegulatorDCDC();  // Switch module from LDO to internal buck regulator.
  g_node.setADR(ENABLE_ADR);   // RadioLib defaults to ADR on, so this must always be set.

  uint64_t joinEUI = hexToU64(TTN_JOIN_EUI_HEX);
  uint64_t devEUI = hexToU64(TTN_DEV_EUI_HEX);
  uint8_t appKey[16] = {};
  uint8_t nwkKey[16] = {};
  hexToBytes(TTN_APP_KEY_HEX, appKey, 16);
#ifdef TTN_NWK_KEY_HEX
  hexToBytes(TTN_NWK_KEY_HEX, nwkKey, 16);
#else
  memcpy(nwkKey, appKey, 16);
#endif

  g_node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

#if ENABLE_SESSION_PERSISTENCE
  if (sessionSaved()) {
    g_node.setBufferNonces(g_savedNonces);
    g_node.setBufferSession(g_savedSession);
  }
#endif

  state = g_node.activateOTAA();
  if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    g_sessionStatus = "RESTORED";
  } else if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    g_sessionStatus = "NEW_JOINED";
  } else if (state == RADIOLIB_ERR_NONE) {
    g_sessionStatus = "ACTIVE";
  } else {
    g_sessionStatus = "FAILED";
    DBG_PRINTF("[LoRaWAN TX ERROR] Activation failed with code %d\n", state);
    return false;
  }

#if ENABLE_SESSION_PERSISTENCE
  if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    memcpy(g_savedNonces, g_node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    memcpy(g_savedSession, g_node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    g_sessionMagic = SESSION_MAGIC;
  }
#endif

  watchdogFeed();

  DBG_PRINTF("[LoRaWAN TX] Activation State: %s (code %d)\n", g_sessionStatus, state);
  DBG_PRINTF("[LoRaWAN TX] FCntUp: %lu | FPort: %u | Length: %u bytes\n", (unsigned long)g_node.getFCntUp(), UPLINK_FPORT, (unsigned)len);
  DBG_PRINTF("[LoRaWAN TX] Hex Payload: [%s]\n", g_lastPayloadHex);
  DBG_PRINTLN("[LoRaWAN TX] Transmitting packet to Gateway...");

  int16_t sendState = g_node.sendReceive(payload, len, UPLINK_FPORT);

#if ENABLE_SESSION_PERSISTENCE
  memcpy(g_savedSession, g_node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
#endif

#if ENABLE_BATTERY_SAG
  batterySampleSag();
#endif

  DBG_PRINTF("[LoRaWAN TX] Result: %s (code %d)\n", (sendState == RADIOLIB_ERR_NONE) ? "SUCCESS" : "ERROR", sendState);
  DBG_PRINTF("[LoRaWAN TX] Battery: Rest=%u mV | Sag=%u mV\n", batteryRestMv(), batterySagMv());
  DBG_PRINTLN("----------------------------------------------\n");

  watchdogFeed();
  return sendState == RADIOLIB_ERR_NONE;
}

bool lorawanSendEvent(UplinkEvent event, float peakTiltDeg, float restTiltDeg, uint32_t openMs) {
  UplinkData d = {batteryRestMv(), batterySagMv(), peakTiltDeg, restTiltDeg, openMs, temperatureC(), event};
  uint8_t payload[UPLINK_SIZE];
  encodeUplink(d, payload);
  return sendPacket(payload, sizeof(payload));
}

uint32_t lorawanFrameCounter() {
  return g_node.getFCntUp();
}

const char* lorawanSessionStatus() {
  return g_sessionStatus;
}

const char* lorawanLastPayloadHex() {
  return g_lastPayloadHex;
}

void lorawanSleep() {
  g_radio.sleep();
}
