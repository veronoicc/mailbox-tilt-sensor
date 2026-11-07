#include <unity.h>

#include "lora/uplink.h"

void setUp() {}
void tearDown() {}

static UplinkMsg withHeader(UplinkMsg m, uint16_t batteryMv, uint16_t sagMv, int8_t tempC) {
  m.batteryMv = batteryMv;
  m.sagMv = sagMv;
  m.temperatureC = tempC;
  return m;
}

static void check(const UplinkMsg& m, const uint8_t* want, size_t n) {
  uint8_t out[UPLINK_MAX_SIZE] = {0};
  TEST_ASSERT_EQUAL_UINT(n, encodeUplink(m, out));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, n);
}

// Byte vectors mirror ttn/payload_formatter.test.js — keep both sides in sync.
static void test_mail_inserted() {
  const uint8_t want[] = {0x01, 101, 9, 18, 48, 0, 8};
  check(withHeader(UplinkMsg::mailInserted(48.4f, 0.0f, 850), 3010, 45, 18), want, sizeof want);
}

static void test_clamps_low_and_high() {
  const uint8_t want[] = {0x01, 0, 255, 0xF4, 90, 0, 255};
  check(withHeader(UplinkMsg::mailInserted(120.0f, -5.0f, 60000), 1900, 2000, -12), want, sizeof want);
}

static void test_rounding_and_battery_cap() {
  const uint8_t want[] = {0x04, 255, 0, 0, 13, 90, 0};
  check(withHeader(UplinkMsg::manualTest(12.5f, 89.6f), 5000, 0, 0), want, sizeof want);
}

static void test_mail_cleared() {
  const uint8_t want[] = {0x02, 101, 0, 18, 2};
  check(withHeader(UplinkMsg::mailCleared(ClearSource::Console), 3010, 0, 18), want, sizeof want);
}

static void test_heartbeat() {
  const uint8_t want[] = {0x03, 101, 9, 18, 1, 1};
  check(withHeader(UplinkMsg::heartbeat(true, HeartbeatReason::Startup), 3010, 45, 18), want, sizeof want);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mail_inserted);
  RUN_TEST(test_clamps_low_and_high);
  RUN_TEST(test_rounding_and_battery_cap);
  RUN_TEST(test_mail_cleared);
  RUN_TEST(test_heartbeat);
  return UNITY_END();
}
