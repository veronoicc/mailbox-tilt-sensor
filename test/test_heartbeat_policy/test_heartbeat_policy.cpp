#include <unity.h>

#include "core/heartbeat_policy.h"

void setUp() {}
void tearDown() {}

static void test_first_startup() { TEST_ASSERT_TRUE(heartbeatOnStartup(true, true, 1, 3)); }

static void test_exactly_at_max() { TEST_ASSERT_TRUE(heartbeatOnStartup(true, true, 3, 3)); }

static void test_over_max() { TEST_ASSERT_FALSE(heartbeatOnStartup(true, true, 4, 3)); }

static void test_flap_wake() { TEST_ASSERT_FALSE(heartbeatOnStartup(true, false, 0, 3)); }

static void test_disabled() { TEST_ASSERT_FALSE(heartbeatOnStartup(false, true, 1, 3)); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_startup);
  RUN_TEST(test_exactly_at_max);
  RUN_TEST(test_over_max);
  RUN_TEST(test_flap_wake);
  RUN_TEST(test_disabled);
  return UNITY_END();
}
