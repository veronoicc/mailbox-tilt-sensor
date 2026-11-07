#include <unity.h>

#include "core/retry_policy.h"

void setUp() {}
void tearDown() {}

static void test_not_sent_retries_until_budget_spent() {
  TEST_ASSERT_TRUE(sendShouldRetry(SendOutcome::NotSent, 1, 3));
  TEST_ASSERT_TRUE(sendShouldRetry(SendOutcome::NotSent, 3, 3));
  TEST_ASSERT_FALSE(sendShouldRetry(SendOutcome::NotSent, 4, 3));  // 1 attempt + 3 retries
}

static void test_zero_retries_disables() { TEST_ASSERT_FALSE(sendShouldRetry(SendOutcome::NotSent, 1, 0)); }

static void test_transmitted_uplink_never_repeats() {
  TEST_ASSERT_FALSE(sendShouldRetry(SendOutcome::MaybeSent, 1, 3));
  TEST_ASSERT_FALSE(sendShouldRetry(SendOutcome::Sent, 1, 3));
}

static void test_permanent_refusal_never_retries() { TEST_ASSERT_FALSE(sendShouldRetry(SendOutcome::NotSentPermanent, 1, 3)); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_not_sent_retries_until_budget_spent);
  RUN_TEST(test_zero_retries_disables);
  RUN_TEST(test_transmitted_uplink_never_repeats);
  RUN_TEST(test_permanent_refusal_never_retries);
  return UNITY_END();
}
