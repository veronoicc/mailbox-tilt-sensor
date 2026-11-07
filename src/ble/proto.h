#pragma once
// Binary device->browser framing: 0xA5, type, len, payload[len], crc8 (poly 0x07 over type,len,payload).
// All values little-endian. Mirrored by web/proto.js. Encoders return the frame length.
#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t PROTO_SYNC = 0xA5;
static constexpr size_t PROTO_MAX_PAYLOAD = 64;
static constexpr size_t PROTO_MAX_FRAME = PROTO_MAX_PAYLOAD + 4;
static constexpr size_t PROTO_MAX_REPLY = 60;  // message bytes in a REPLY

enum ProtoType : uint8_t { PROTO_TELEMETRY = 1, PROTO_KNOCK = 2, PROTO_REPLY = 3, PROTO_STATUS = 4 };
enum ProtoComp : uint8_t { COMP_POWER = 1, COMP_IMU = 2, COMP_LORA = 3, COMP_KNOCK = 4, COMP_SYSTEM = 5 };

uint8_t protoCrc8(const uint8_t* data, size_t len);

// Round to nearest, saturating at the wire type's range.
int16_t protoI16(float v);
uint16_t protoU16(float v);

size_t protoTelemetry(uint8_t* b, uint16_t vLiveMv, int8_t tempC, int16_t pitchX10, int16_t axMg,
                      int16_t ayMg, int16_t azMg, uint16_t magMg, uint16_t knocks, bool mailInside);
size_t protoKnock(uint8_t* b, uint16_t count, uint16_t magMg);
size_t protoReply(uint8_t* b, bool ok, const char* msg);  // msg truncated to PROTO_MAX_REPLY

size_t protoStatusPower(uint8_t* b, uint16_t restMv, uint16_t sagMv, uint16_t liveMv, int8_t tempC);
size_t protoStatusImu(uint8_t* b, bool present, int16_t pitchX10, int16_t axMg, int16_t ayMg,
                      int16_t azMg, uint16_t magMg, int16_t zeroX10);
// Strings are truncated so the frame stays within PROTO_MAX_PAYLOAD.
size_t protoStatusLora(uint8_t* b, uint32_t fcnt, const char* session, const char* payloadHex);
size_t protoStatusKnock(uint8_t* b, uint16_t count);
size_t protoStatusSystem(uint8_t* b, bool bench, bool usb, bool mailInside, uint16_t streamMs, const char* fw);
