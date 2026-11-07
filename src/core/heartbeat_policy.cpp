#include "core/heartbeat_policy.h"

bool heartbeatOnStartup(bool enabled, bool realStartup, uint8_t consecutiveStartups, uint8_t rapidRebootMax) {
  return enabled && realStartup && consecutiveStartups <= rapidRebootMax;
}
