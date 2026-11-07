#include <unity.h>

#include "lora/uplink.h"

void setUp() {}
void tearDown() {}

// Fills what lorawanSend() normally fills: battery, sag, temperature, mail state, and tilt for non-flap events.
static UplinkMsg filled(UplinkMsg m, uint16_t batteryMv, uint16_t sagMv, int8_t tempC, bool inside) {
  m.batteryMv = batteryMv;
  m.sagMv = sagMv;
  m.temperatureC = tempC;
  m.mailInside = inside;
  return m;
}

static void check(const UplinkMsg& m, const uint8_t (&want)[UPLINK_SIZE]) {
  uint8_t out[UPLINK_SIZE] = {0};
  encodeUplink(m, out);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, UPLINK_SIZE);
}

// Byte vectors mirror ttn/formatters.test.js — keep both sides in sync.
static void test_mail_inserted() {
  const uint8_t want[] = {0x01, 101, 9, 18, 1, 48, 0, 8};
  check(filled(UplinkMsg::mailInserted(48.4f, 0.0f, 850), 3010, 45, 18, true), want);
}

static void test_clamps_low_and_high() {
  const uint8_t want[] = {0x01, 0, 255, 0xF4, 0, 90, 0, 255};
  check(filled(UplinkMsg::mailInserted(120.0f, -5.0f, 60000), 1900, 2000, -12, false), want);
}

static void test_rounding_and_battery_cap() {
  const uint8_t want[] = {0x07, 255, 0, 0, 0, 13, 90, 0};
  UplinkMsg m = UplinkMsg::of(UplinkReason::ManualTest);
  m.peakTiltDeg = 12.5f;
  m.restTiltDeg = 89.6f;
  check(filled(m, 5000, 0, 0, false), want);
}

static void test_non_flap_reasons_carry_full_frame() {
  UplinkMsg m = UplinkMsg::of(UplinkReason::HeartbeatRequested);
  m.peakTiltDeg = m.restTiltDeg = 3.0f;
  const uint8_t want[] = {0x04, 101, 9, 18, 1, 3, 3, 0};
  check(filled(m, 3010, 45, 18, true), want);
}

static void test_cleared_reports_empty() {
  const uint8_t want[] = {0x02, 101, 0, 18, 0, 0, 0, 0};
  check(filled(UplinkMsg::of(UplinkReason::MailCleared), 3010, 0, 18, false), want);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mail_inserted);
  RUN_TEST(test_clamps_low_and_high);
  RUN_TEST(test_rounding_and_battery_cap);
  RUN_TEST(test_non_flap_reasons_carry_full_frame);
  RUN_TEST(test_cleared_reports_empty);
  return UNITY_END();
}
