"use strict";

// Nordic UART Service. RX = browser -> device (write), TX = device -> browser (notify).
const NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_TX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

const $ = (id) => document.getElementById(id);

const ui = {};
[
  "browserModal", "btnCloseModal", "btnDismissModal", "deviceStatus",
  "disconnectedView", "connectedView", "btnConnect", "btnDisconnect",
  "btnRecalibrate", "btnTestUplink", "btnResetKnocks", "btnDfu", "btnHeartbeat", "btnClear",
  "inputPin", "logView", "btnClearLog",
  "valVRest", "valVSag", "valVLive", "valBatHealth", "valTilt", "valTiltTag",
  "valAccel", "valMag", "valZero", "valKnocks", "valKnockPeak",
  "knockPulseIndicator", "valLoraSession", "valLoraTag", "valFCnt",
  "valLastPayload", "valTemp", "valTempTag", "firmwareVersion", "valMailInside",
].forEach((id) => { ui[id] = $(id); });

let device = null;
let rxChar = null;
let lineBuffer = "";
let knockTimer = null;
const decoder = new TextDecoder();
const encoder = new TextEncoder();

function log(line) {
  ui.logView.textContent += "\n" + line;
  ui.logView.scrollTop = ui.logView.scrollHeight;
}

function setStatus(text, state) {
  ui.deviceStatus.textContent = text;
  ui.deviceStatus.dataset.state = state;
}

function showConnected(isConnected) {
  ui.disconnectedView.hidden = isConnected;
  ui.connectedView.hidden = !isConnected;
}

function batteryHealth(mv) {
  if (mv >= 2800) return "Optimal (>2.8V)";
  if (mv >= 2400) return "Low (<2.8V)";
  return "Depleted (<2.4V)";
}

async function connect() {
  if (!("bluetooth" in navigator)) {
    ui.browserModal.hidden = false;
    return;
  }

  try {
    setStatus("connecting...", "disconnected");
    log("Scanning for Bluetooth peripheral...");

    device = await navigator.bluetooth.requestDevice({
      filters: [
        { services: [NUS_SERVICE_UUID] },
        { namePrefix: "Mailbox" },
      ],
      optionalServices: [NUS_SERVICE_UUID],
    });
    device.addEventListener("gattserverdisconnected", onDisconnected);

    log("Connecting to GATT service...");
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(NUS_SERVICE_UUID);

    rxChar = await service.getCharacteristic(NUS_RX_CHAR_UUID);
    const txChar = await service.getCharacteristic(NUS_TX_CHAR_UUID);

    await txChar.startNotifications();
    txChar.addEventListener("characteristicvaluechanged", onNotify);

    setStatus("connected", "connected");
    showConnected(true);
    log("Connected. Live telemetry stream active.");
  } catch (err) {
    setStatus("connection failed", "error");
    log("Connection error: " + err.message);
  }
}

function onDisconnected() {
  setStatus("disconnected", "disconnected");
  showConnected(false);
  rxChar = null;
  log("Device disconnected.");
}

function onNotify(event) {
  lineBuffer += decoder.decode(event.target.value, { stream: true });
  const lines = lineBuffer.split("\n");
  lineBuffer = lines.pop();
  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed) handleLine(trimmed);
  }
}

function handleLine(line) {
  try {
    const data = JSON.parse(line);
    if (data.type === "telemetry") renderTelemetry(data);
    else if (data.type === "knock") renderKnock(data);
    else log(`[RECV] ${line}`);
  } catch (e) {
    log(`[RECV RAW] ${line}`);
  }
}

function renderTelemetry(t) {
  ui.valVRest.textContent = (t.v_rest / 1000).toFixed(2);
  ui.valVSag.textContent = t.v_sag;
  ui.valVLive.textContent = (t.v_live / 1000).toFixed(2) + " V";
  ui.valBatHealth.textContent = batteryHealth(t.v_rest);

  const imuAttached = t.ax !== 0 || t.ay !== 0 || t.az !== 0 || t.mag !== 0;
  if (imuAttached) {
    ui.valTilt.textContent = t.tilt.toFixed(1);
    ui.valTiltTag.textContent = t.tilt.toFixed(1) + "°";
    ui.valAccel.textContent = `X:${t.ax.toFixed(2)} Y:${t.ay.toFixed(2)} Z:${t.az.toFixed(2)}`;
    ui.valMag.textContent = t.mag.toFixed(2);
  } else {
    ui.valTilt.textContent = "0.0";
    ui.valTiltTag.textContent = "No IMU";
    ui.valAccel.textContent = "Not connected (D4/D5)";
    ui.valMag.textContent = "0.00";
  }
  ui.valZero.textContent = t.zero.toFixed(1);

  ui.valKnocks.textContent = t.knocks;

  ui.valLoraSession.textContent = t.lora_session;
  ui.valLoraTag.textContent = t.lora_session;
  ui.valFCnt.textContent = t.fcnt;
  ui.valMailInside.textContent = t.mail_inside ? "Yes" : "No";
  ui.valLastPayload.textContent = t.last_payload || "None";

  ui.valTemp.textContent = t.temp;
  ui.valTempTag.textContent = t.temp + " °C";
  ui.firmwareVersion.textContent = "v" + t.fw;

  log(`[TELEMETRY] Pitch: ${t.tilt}° | V_live: ${(t.v_live / 1000).toFixed(2)}V | Knocks: ${t.knocks} | FCnt: ${t.fcnt}`);
}

function renderKnock(k) {
  ui.valKnocks.textContent = k.count;
  ui.valKnockPeak.textContent = k.mag.toFixed(2);
  ui.knockPulseIndicator.textContent = `Hit #${k.count} (${k.mag}g)`;
  ui.knockPulseIndicator.dataset.hit = "active";

  clearTimeout(knockTimer);
  knockTimer = setTimeout(() => {
    ui.knockPulseIndicator.dataset.hit = "idle";
    ui.knockPulseIndicator.textContent = "Ready";
  }, 600);

  log(`*** [KNOCK HIT] #${k.count} registered | Peak force: ${k.mag}g ***`);
}

async function sendCommand(action) {
  if (!rxChar) {
    log("Error: Not connected to peripheral.");
    return;
  }
  const pin = ui.inputPin.value.trim();
  log(`[SEND] CMD:${action}`);
  try {
    await rxChar.writeValue(encoder.encode(`PIN:${pin} CMD:${action}\n`));
  } catch (err) {
    log(`[SEND ERROR] ${err.message}`);
  }
}

if (!("bluetooth" in navigator)) {
  ui.browserModal.hidden = false;
  setStatus("Unsupported browser", "error");
  log("Notice: Web Bluetooth API is not supported in this browser.");
}

const dismissModal = () => { ui.browserModal.hidden = true; };
ui.btnCloseModal.addEventListener("click", dismissModal);
ui.btnDismissModal.addEventListener("click", dismissModal);
ui.btnClearLog.addEventListener("click", () => { ui.logView.textContent = "Logs cleared."; });
ui.btnConnect.addEventListener("click", connect);

const commandButtons = {
  btnRecalibrate: "RECALIBRATE",
  btnTestUplink: "TEST_UPLINK",
  btnResetKnocks: "RESET_KNOCKS",
  btnDfu: "DFU",
  btnHeartbeat: "HEARTBEAT",
  btnClear: "CLEAR",
};
for (const [id, action] of Object.entries(commandButtons)) {
  ui[id].addEventListener("click", () => sendCommand(action));
}

ui.btnDisconnect.addEventListener("click", async () => {
  await sendCommand("SLEEP");
  if (device && device.gatt.connected) device.gatt.disconnect();
});
