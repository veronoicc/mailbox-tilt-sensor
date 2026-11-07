#include <string.h>
#include <unity.h>

#include "cmd/cmd_parse.h"

void setUp() {}
void tearDown() {}

static void test_ble_form() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("PIN:123456 CMD:test_uplink", p));
  TEST_ASSERT_TRUE(p.hasPin);
  TEST_ASSERT_EQUAL_STRING("123456", p.pin);
  TEST_ASSERT_EQUAL_STRING("TEST_UPLINK", p.name);
  TEST_ASSERT_EQUAL_UINT8(0, p.argc);
}

static void test_ble_form_with_args_and_newline() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("PIN:1 CMD:help foo Bar\r\n", p));
  TEST_ASSERT_EQUAL_STRING("HELP", p.name);
  TEST_ASSERT_EQUAL_UINT8(2, p.argc);
  TEST_ASSERT_EQUAL_STRING("foo", p.args[0]);
  TEST_ASSERT_EQUAL_STRING("Bar", p.args[1]);  // args keep their case
}

static void test_prefix_case_and_order() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("cmd:sleep pin:42", p));
  TEST_ASSERT_TRUE(p.hasPin);
  TEST_ASSERT_EQUAL_STRING("42", p.pin);
  TEST_ASSERT_EQUAL_STRING("SLEEP", p.name);
}

static void test_cli_form() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("  test_uplink a1   a2 ", p));
  TEST_ASSERT_FALSE(p.hasPin);
  TEST_ASSERT_EQUAL_STRING("TEST_UPLINK", p.name);
  TEST_ASSERT_EQUAL_UINT8(2, p.argc);
  TEST_ASSERT_EQUAL_STRING("a1", p.args[0]);
  TEST_ASSERT_EQUAL_STRING("a2", p.args[1]);
}

static void test_empty_and_null() {
  ParsedLine p;
  TEST_ASSERT_FALSE(cmdParseLine("", p));
  TEST_ASSERT_FALSE(cmdParseLine("   \r\n", p));
  TEST_ASSERT_FALSE(cmdParseLine(nullptr, p));
}

static void test_missing_cmd() {
  ParsedLine p;
  TEST_ASSERT_FALSE(cmdParseLine("PIN:123456", p));
  TEST_ASSERT_FALSE(cmdParseLine("PIN:123456 CMD:", p));
  TEST_ASSERT_FALSE(cmdParseLine("PIN:123456 test_uplink", p));  // BLE form needs CMD:
}

static void test_too_many_args() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("x a b c d", p));
  TEST_ASSERT_EQUAL_UINT8(4, p.argc);
  TEST_ASSERT_FALSE(cmdParseLine("x a b c d e", p));
}

static void test_field_bounds() {
  ParsedLine p;
  // pin holds 15 chars, name 23, each arg 23 (plus NUL).
  TEST_ASSERT_TRUE(cmdParseLine("PIN:123456789012345 CMD:abcdefghijklmnopqrstuvw", p));
  TEST_ASSERT_EQUAL_UINT(15, strlen(p.pin));
  TEST_ASSERT_EQUAL_UINT(23, strlen(p.name));
  TEST_ASSERT_FALSE(cmdParseLine("PIN:1234567890123456 CMD:x", p));
  TEST_ASSERT_FALSE(cmdParseLine("PIN:1 CMD:abcdefghijklmnopqrstuvwx", p));
  TEST_ASSERT_FALSE(cmdParseLine("x abcdefghijklmnopqrstuvwx", p));
  TEST_ASSERT_TRUE(cmdParseLine("x abcdefghijklmnopqrstuvw", p));
}

static void test_oversized_line() {
  char big[CMD_LINE_MAX + 40];
  memset(big, 'a', sizeof big - 1);
  big[sizeof big - 1] = 0;
  ParsedLine p;
  TEST_ASSERT_FALSE(cmdParseLine(big, p));
  // Whitespace padding past the limit is rejected too.
  memset(big, ' ', sizeof big - 1);
  memcpy(big + sizeof big - 5, "help", 5);
  TEST_ASSERT_FALSE(cmdParseLine(big, p));
}

static void test_output_cleared_on_failure() {
  ParsedLine p;
  TEST_ASSERT_TRUE(cmdParseLine("PIN:1 CMD:a b", p));
  TEST_ASSERT_FALSE(cmdParseLine("PIN:1", p));
  TEST_ASSERT_EQUAL_UINT8(0, p.argc);
  TEST_ASSERT_EQUAL_CHAR(0, p.name[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ble_form);
  RUN_TEST(test_ble_form_with_args_and_newline);
  RUN_TEST(test_prefix_case_and_order);
  RUN_TEST(test_cli_form);
  RUN_TEST(test_empty_and_null);
  RUN_TEST(test_missing_cmd);
  RUN_TEST(test_too_many_args);
  RUN_TEST(test_field_bounds);
  RUN_TEST(test_oversized_line);
  RUN_TEST(test_output_cleared_on_failure);
  return UNITY_END();
}
