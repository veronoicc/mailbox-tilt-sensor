// LoRaWAN OTAA join/restore and uplink on the SX1262.
#include "lora/lorawan.h"
#include <Arduino.h>
#include <RadioLib.h>
#include "core/store.h"
#include <string.h>
#include "board.h"
#include "settings.h"
#include "core/log.h"
#include "core/power.h"
#include "core/retry_policy.h"
#include "core/state.h"
#include "motion/imu.h"
#include "lora/hex_key.h"

static SX1262 g_radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);
static LoRaWANNode g_node(&g_radio, &LORAWAN_BAND);

static const char* g_sessionStatus = "COLD_BOOT";
static char g_lastPayloadHex[48] = "None";

#if ENABLE_SESSION_PERSISTENCE
static constexpr uint32_t SESSION_MAGIC = 0xA55A1234;
__attribute__((section(".noinit"))) static uint32_t g_sessionMagic;
__attribute__((section(".noinit"))) static uint8_t g_savedSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
#endif

static bool sessionSaved() {
#if ENABLE_SESSION_PERSISTENCE
  return g_sessionMagic == SESSION_MAGIC;
#else
  return false;
#endif
}

// TTN rejects a reused DevNonce (LoRaWAN 1.0.4) and RAM is lost on power-off, so keep the nonces in flash.
static const char* NONCES_FILE = "/lw_nonces";

// One pending downlink from the RX windows, handed out by lorawanTakeDownlink().
static uint8_t g_dlBuf[16];
static size_t g_dlLen = 0;
static uint8_t g_dlPort = 0;
static bool g_dlPending = false;

// First (up to) 12 bytes as space-separated "%02X".
static void formatHex(const uint8_t* payload, size_t len) {
  char* p = g_lastPayloadHex;
  for (size_t i = 0; i < len && i < 12; i++) {
    p += sprintf(p, "%02X ", payload[i]);
  }
  if (p > g_lastPayloadHex) *(p - 1) = '\0';
}

// RadioLib 6.6 fills the nonces buffer header (version, mode, class, plan, key checksum) only after a
// SUCCESSFUL join, so setBufferNonces() would reject the buffer saved after a failed join and the
// DevNonce would restart at 0. Write the header ourselves, then let RadioLib re-sign the buffer.
// beginOTAA() leaves RadioLib's keyCheckSum at 0 (clearNonces() runs after it is computed), so the
// checksum field is 0; revisit this if RadioLib is upgraded past 6.6.x.
static uint8_t* signedNonces() {
  uint8_t* n = g_node.getBufferNonces();
  // RadioLib stores multi-byte fields little-endian.
  n[RADIOLIB_LORAWAN_NONCES_VERSION] = RADIOLIB_LORAWAN_NONCES_VERSION_VAL & 0xFF;
  n[RADIOLIB_LORAWAN_NONCES_VERSION + 1] = RADIOLIB_LORAWAN_NONCES_VERSION_VAL >> 8;
  n[RADIOLIB_LORAWAN_NONCES_MODE] = RADIOLIB_LORAWAN_MODE_OTAA & 0xFF;
  n[RADIOLIB_LORAWAN_NONCES_MODE + 1] = RADIOLIB_LORAWAN_MODE_OTAA >> 8;
  n[RADIOLIB_LORAWAN_NONCES_CLASS] = RADIOLIB_LORAWAN_CLASS_A;
  n[RADIOLIB_LORAWAN_NONCES_PLAN] = LORAWAN_BAND.bandNum;
  n[RADIOLIB_LORAWAN_NONCES_CHECKSUM] = 0;
  n[RADIOLIB_LORAWAN_NONCES_CHECKSUM + 1] = 0;
  return g_node.getBufferNonces();  // recomputes the signature over the patched header
}

static constexpr uint32_t RETRY_DELAY_MS = 5000;  // well under the 30 s watchdog

// Uplink() refusals that waiting won't fix (duty cycle needs far longer than RETRY_DELAY_MS).
static bool permanentUplinkError(int16_t s) {
  return s == RADIOLIB_ERR_UPLINK_UNAVAILABLE || s == RADIOLIB_ERR_INVALID_PORT ||
         s == RADIOLIB_ERR_PACKET_TOO_LONG || s == RADIOLIB_ERR_DWELL_TIME_EXCEEDED;
}

static SendOutcome sendPacket(uint8_t* payload, size_t len) {
  watchdogFeed();
  formatHex(payload, len);

  DBG_PRINTLN("\n----------------------------------------------");
  DBG_PRINTLN("[LoRaWAN TX] Preparing Uplink Transmission");
  DBG_PRINTF("[LoRaWAN TX] JoinEUI: %s | DevEUI: %s\n", TTN_JOIN_EUI_HEX, TTN_DEV_EUI_HEX);
  DBG_PRINTF("[LoRaWAN TX] Retained RAM Session: %s\n",
             sessionSaved() ? "VALID (Restoring Session)" : "COLD_BOOT (Full OTAA Join Needed)");

  g_radio.setRfSwitchPins(PIN_LORA_RXEN, RADIOLIB_NC);
  int16_t state = g_radio.begin(434.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8,
                                LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    DBG_PRINTF("[LoRaWAN TX ERROR] Radio begin failed: %d\n", state);
    return SendOutcome::NotSent;
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

  uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
  if (storeRead(NONCES_FILE, nonces, sizeof(nonces))) {
    int16_t ns = g_node.setBufferNonces(nonces);
    DBG_PRINTF("[LoRaWAN TX] Nonces restored from flash (code %d)\n", ns);
  } else {
    DBG_PRINTLN("[LoRaWAN TX] No saved nonces in flash, DevNonce starts at 0");
  }
#if ENABLE_SESSION_PERSISTENCE
  if (sessionSaved()) g_node.setBufferSession(g_savedSession);
#endif

  state = g_node.activateOTAA();
  // Save after every join attempt, even a failed one: the request already burned its DevNonce.
  if (state != RADIOLIB_LORAWAN_SESSION_RESTORED) storeWrite(NONCES_FILE, signedNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    g_sessionStatus = "RESTORED";
  } else if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    g_sessionStatus = "NEW_JOINED";
  } else if (state == RADIOLIB_ERR_NONE) {
    g_sessionStatus = "ACTIVE";
  } else {
    g_sessionStatus = "FAILED";
    DBG_PRINTF("[LoRaWAN TX ERROR] Activation failed with code %d\n", state);
    return SendOutcome::NotSent;
  }

#if ENABLE_SESSION_PERSISTENCE
  if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    memcpy(g_savedSession, g_node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    g_sessionMagic = SESSION_MAGIC;
  }
#endif

  watchdogFeed();

  DBG_PRINTF("[LoRaWAN TX] Activation State: %s (code %d)\n", g_sessionStatus, state);
  DBG_PRINTF("[LoRaWAN TX] FCntUp: %lu | FPort: %u | Length: %u bytes\n", (unsigned long)g_node.getFCntUp(), UPLINK_FPORT, (unsigned)len);
  DBG_PRINTF("[LoRaWAN TX] Hex Payload: [%s]\n", g_lastPayloadHex);
  DBG_PRINTLN("[LoRaWAN TX] Transmitting packet to Gateway...");

  uint8_t dataDown[255];  // max LoRaWAN payload; RadioLib copies the whole downlink without a bound check
  size_t lenDown = 0;
  LoRaWANEvent_t eventUp = {};
  LoRaWANEvent_t eventDown = {};
  // Uplink and downlink run as two calls (what sendReceive does) so a failure can be pinned to a stage.
  int16_t sendState = g_node.uplink(payload, len, UPLINK_FPORT, false, &eventUp);
  bool transmitted = sendState == RADIOLIB_ERR_NONE;
  if (transmitted) {
    sendState = g_node.downlink(dataDown, &lenDown, &eventDown);
    if (sendState > 0 && lenDown > 0) {
      g_dlPort = eventDown.fPort;
      g_dlLen = lenDown < sizeof(g_dlBuf) ? lenDown : sizeof(g_dlBuf);
      memcpy(g_dlBuf, dataDown, g_dlLen);
      g_dlPending = true;
      DBG_PRINTF("[LoRaWAN RX] Downlink FPort %u, %u bytes\n", g_dlPort, (unsigned)lenDown);
    }
  }

#if ENABLE_SESSION_PERSISTENCE
  memcpy(g_savedSession, g_node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
#endif

#if ENABLE_BATTERY_SAG
  batterySampleSag();
#endif

  // -1116 (NO_DOWNLINK) just means the uplink went out and no downlink came back, which is normal for unconfirmed uplinks.
  bool sent = sendState >= 0 || sendState == RADIOLIB_LORAWAN_NO_DOWNLINK;
  DBG_PRINTF("[LoRaWAN TX] Result: %s (code %d)\n", sent ? "SUCCESS" : "ERROR", sendState);
  DBG_PRINTF("[LoRaWAN TX] Battery: Rest=%u mV | Sag=%u mV\n", batteryRestMv(), batterySagMv());
  DBG_PRINTLN("----------------------------------------------\n");

  watchdogFeed();
  if (sent) return SendOutcome::Sent;
  if (transmitted) return SendOutcome::MaybeSent;
  return permanentUplinkError(sendState) ? SendOutcome::NotSentPermanent : SendOutcome::NotSent;
}

bool lorawanSend(UplinkMsg msg) {
  msg.batteryMv = batteryRestMv();
  msg.sagMv = batterySagMv();
  msg.temperatureC = temperatureC();
  msg.mailInside = stateMailInside();
  if (!msg.hasTilt) msg.peakTiltDeg = msg.restTiltDeg = imuRead().pitch;
  uint8_t payload[UPLINK_SIZE];
  encodeUplink(msg, payload);
  for (uint8_t attempts = 1;; attempts++) {
    SendOutcome outcome = sendPacket(payload, UPLINK_SIZE);
    if (outcome == SendOutcome::Sent) return true;
    if (!sendShouldRetry(outcome, attempts, LORAWAN_MAX_RETRIES)) return false;
    DBG_PRINTF("[LoRaWAN TX] Retry %u/%u in %u ms\n", attempts, (unsigned)LORAWAN_MAX_RETRIES, (unsigned)RETRY_DELAY_MS);
    watchdogFeed();
    delay(RETRY_DELAY_MS);
    watchdogFeed();
  }
}

bool lorawanTakeDownlink(uint8_t& fPort, uint8_t* buf, size_t cap, size_t& len) {
  if (!g_dlPending) return false;
  g_dlPending = false;
  fPort = g_dlPort;
  len = g_dlLen < cap ? g_dlLen : cap;
  memcpy(buf, g_dlBuf, len);
  return true;
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
