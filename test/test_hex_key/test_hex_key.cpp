#include <string.h>
#include <unity.h>

#include "lora/hex_key.h"

void setUp() {}
void tearDown() {}

static const uint64_t EUI = 0x70B3D57ED0060000ULL;
static const uint8_t KEY[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static void test_u64_plain() { TEST_ASSERT_TRUE(hexToU64("70B3D57ED0060000") == EUI); }

static void test_u64_separators_and_array_style() {
  TEST_ASSERT_TRUE(hexToU64("70-B3:D5 7e d0 06 00 00") == EUI);
  TEST_ASSERT_TRUE(hexToU64("{ 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x06, 0x00, 0x00 }") == EUI);
}

static void test_u64_prefix() { TEST_ASSERT_TRUE(hexToU64("0x70B3D57ED0060000") == EUI); }

static void test_u64_short() { TEST_ASSERT_TRUE(hexToU64("1234") == 0x1234ULL); }

static void test_bytes_plain() {
  uint8_t buf[16];
  TEST_ASSERT_EQUAL(16, hexToBytes("00112233445566778899AABBCCDDEEFF", buf, 16));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(KEY, buf, 16);
}

// Regression: a leading "0x" used to be read as a nibble and shift the whole key.
static void test_bytes_prefix() {
  uint8_t buf[16];
  TEST_ASSERT_EQUAL(16, hexToBytes("0x00112233445566778899AABBCCDDEEFF", buf, 16));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(KEY, buf, 16);
}

static void test_bytes_limit() {
  uint8_t buf[16];
  memset(buf, 0xAA, sizeof buf);
  TEST_ASSERT_EQUAL(8, hexToBytes("00112233445566778899AABBCCDDEEFF", buf, 8));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(KEY, buf, 8);
  TEST_ASSERT_EQUAL_HEX8(0xAA, buf[8]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_u64_plain);
  RUN_TEST(test_u64_separators_and_array_style);
  RUN_TEST(test_u64_prefix);
  RUN_TEST(test_u64_short);
  RUN_TEST(test_bytes_plain);
  RUN_TEST(test_bytes_prefix);
  RUN_TEST(test_bytes_limit);
  return UNITY_END();
}
