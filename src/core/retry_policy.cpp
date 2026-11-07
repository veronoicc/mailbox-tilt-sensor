#include "core/retry_policy.h"

bool sendShouldRetry(SendOutcome outcome, uint8_t attemptsDone, uint8_t maxRetries) {
  return outcome == SendOutcome::NotSent && attemptsDone <= maxRetries;
}
