#pragma once
#include <stdint.h>
// How a send attempt ended. Only attempts that never put the uplink on air may be repeated:
// repeating an uplink that went out would deliver the event (e.g. mail_inserted) twice.
enum class SendOutcome : uint8_t {
  Sent,              // uplink transmitted and the RX windows completed normally
  NotSent,           // failed before the uplink was transmitted (radio init, join, TX error); waiting may help
  NotSentPermanent,  // refused before TX for a reason a delay won't fix (duty cycle, bad port, too long, dwell time)
  MaybeSent          // uplink was transmitted, a later (downlink) stage failed
};
// attemptsDone counts attempts already made (the first attempt is 1).
bool sendShouldRetry(SendOutcome outcome, uint8_t attemptsDone, uint8_t maxRetries);
