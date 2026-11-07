#include <string.h>
#include <unity.h>

#include "ble/proto.h"

void setUp() {}
void tearDown() {}

// Byte vectors below are mirrored verbatim in web/proto.test.js.
static void check(const uint8_t* want, size_t wantLen, const uint8_t* got, size_t gotLen) {
  TEST_ASSERT_EQUAL(wantLen, gotLen);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, got, wantLen);
}

static void test_crc() {
  TEST_ASSERT_EQUAL_HEX8(0xF4, protoCrc8((const uint8_t*)"123456789", 9));
}

static void test_saturating_round() {
  TEST_ASSERT_EQUAL_INT16(-123, protoI16(-122.6f));
  TEST_ASSERT_EQUAL_INT16(32767, protoI16(40000.0f));
  TEST_ASSERT_EQUAL_INT16(-32768, protoI16(-40000.0f));
  TEST_ASSERT_EQUAL_UINT16(1000, protoU16(1000.4f));
  TEST_ASSERT_EQUAL_UINT16(0, protoU16(-1.0f));
  TEST_ASSERT_EQUAL_UINT16(65535, protoU16(70000.0f));
}

// telemetry: v=3700 temp=-5 pitch=-123 ax=100 ay=-200 az=980 mag=1000 knocks=7 mail=1
static void test_telemetry() {
  const uint8_t want[] = {0xA5, 0x01, 0x10, 0x74, 0x0E, 0xFB, 0x85, 0xFF, 0x64, 0x00,
                          0x38, 0xFF, 0xD4, 0x03, 0xE8, 0x03, 0x07, 0x00, 0x01, 0x3E};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoTelemetry(b, 3700, -5, -123, 100, -200, 980, 1000, 7, true));
}

// knock: count=3 mag=1500
static void test_knock() {
  const uint8_t want[] = {0xA5, 0x02, 0x04, 0x03, 0x00, 0xDC, 0x05, 0xBA};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoKnock(b, 3, 1500));
}

// reply: ok=1 "OK"
static void test_reply() {
  const uint8_t want[] = {0xA5, 0x03, 0x03, 0x01, 0x4F, 0x4B, 0x99};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoReply(b, true, "OK"));
}

static void test_reply_truncates() {
  char msg[100];
  memset(msg, 'x', sizeof msg - 1);
  msg[sizeof msg - 1] = 0;
  uint8_t b[PROTO_MAX_FRAME];
  size_t n = protoReply(b, false, msg);
  TEST_ASSERT_EQUAL(PROTO_MAX_REPLY + 1 + 4, n);
  TEST_ASSERT_EQUAL_HEX8(0x00, b[3]);
}

// status power: rest=3900 sag=3850 live=3880 temp=21
static void test_status_power() {
  const uint8_t want[] = {0xA5, 0x04, 0x08, 0x01, 0x3C, 0x0F, 0x0A, 0x0F, 0x28, 0x0F, 0x15, 0xBD};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoStatusPower(b, 3900, 3850, 3880, 21));
}

// status imu: present=1 pitch=32 ax=100 ay=-200 az=980 mag=1000 zero=-15
static void test_status_imu() {
  const uint8_t want[] = {0xA5, 0x04, 0x0E, 0x02, 0x01, 0x20, 0x00, 0x64, 0x00, 0x38,
                          0xFF, 0xD4, 0x03, 0xE8, 0x03, 0xF1, 0xFF, 0x1E};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoStatusImu(b, true, 32, 100, -200, 980, 1000, -15));
}

// status lora: fcnt=42 "JOINED" "0A1B"
static void test_status_lora() {
  const uint8_t want[] = {0xA5, 0x04, 0x11, 0x03, 0x2A, 0x00, 0x00, 0x00, 0x06, 0x4A, 0x4F,
                          0x49, 0x4E, 0x45, 0x44, 0x04, 0x30, 0x41, 0x31, 0x42, 0x06};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoStatusLora(b, 42, "JOINED", "0A1B"));
}

// status knock: count=9
static void test_status_knock() {
  const uint8_t want[] = {0xA5, 0x04, 0x03, 0x04, 0x09, 0x00, 0xA3};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoStatusKnock(b, 9));
}

// status system: bench=1 usb=0 mail=1 stream=100 fw="1.2.3"
static void test_status_system() {
  const uint8_t want[] = {0xA5, 0x04, 0x0C, 0x05, 0x01, 0x00, 0x01, 0x64, 0x00, 0x05,
                          0x31, 0x2E, 0x32, 0x2E, 0x33, 0xAC};
  uint8_t b[PROTO_MAX_FRAME];
  check(want, sizeof want, b, protoStatusSystem(b, true, false, true, 100, "1.2.3"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc);
  RUN_TEST(test_saturating_round);
  RUN_TEST(test_telemetry);
  RUN_TEST(test_knock);
  RUN_TEST(test_reply);
  RUN_TEST(test_reply_truncates);
  RUN_TEST(test_status_power);
  RUN_TEST(test_status_imu);
  RUN_TEST(test_status_lora);
  RUN_TEST(test_status_knock);
  RUN_TEST(test_status_system);
  return UNITY_END();
}
