#include <unity.h>

#include "lora/uplink.h"

void setUp() {}
void tearDown() {}

// Byte vectors mirror ttn/payload_formatter.test.js — keep both sides in sync.
static void test_mail_delivered() {
  uint8_t out[UPLINK_SIZE];
  encodeUplink({3010, 45, 48.4f, 0.0f, 850, 18, UplinkEvent::MailDelivered}, out);
  const uint8_t want[] = {101, 9, 48, 0, 8, 18, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, UPLINK_SIZE);
}

static void test_clamps_low_and_high() {
  uint8_t out[UPLINK_SIZE];
  encodeUplink({1900, 2000, 120.0f, -5.0f, 60000, -12, UplinkEvent::MailboxEmptied}, out);
  const uint8_t want[] = {0, 255, 90, 0, 255, 0xF4, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, UPLINK_SIZE);
}

static void test_rounding_and_battery_cap() {
  uint8_t out[UPLINK_SIZE];
  encodeUplink({5000, 0, 12.5f, 89.6f, 0, 0, UplinkEvent::ManualTest}, out);
  const uint8_t want[] = {255, 0, 13, 90, 0, 0, 0x03};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, UPLINK_SIZE);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mail_delivered);
  RUN_TEST(test_clamps_low_and_high);
  RUN_TEST(test_rounding_and_battery_cap);
  return UNITY_END();
}
