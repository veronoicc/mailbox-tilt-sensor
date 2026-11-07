// Paste into TTN Console -> Payload formatters -> Downlink (Custom Javascript formatter).
// Opcodes must match src/cmd/cmd.cpp.
var DOWNLINK_ACTIONS = { heartbeat: 1, clear: 2 };

function encodeDownlink(input) {
  var action = input.data && input.data.action;
  if (!DOWNLINK_ACTIONS.hasOwnProperty(action)) {
    return { errors: ["Unknown action: " + action] };
  }
  return { bytes: [DOWNLINK_ACTIONS[action]], fPort: 2 };
}

function decodeDownlink(input) {
  if (input.fPort !== 2) {
    return { errors: ["Unsupported FPort: " + input.fPort] };
  }
  if (input.bytes.length < 1) {
    return { errors: ["Empty payload"] };
  }
  for (var action in DOWNLINK_ACTIONS) {
    if (DOWNLINK_ACTIONS[action] === input.bytes[0]) {
      return { data: { action: action } };
    }
  }
  return { errors: ["Unknown opcode: " + input.bytes[0]] };
}
