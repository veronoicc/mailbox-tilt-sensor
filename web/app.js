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
  "btnStatus", "inputStream", "btnStream", "valBench", "valUsb",
].forEach((id) => { ui[id] = $(id); });

let device = null;
let rxChar = null;
let knockTimer = null;
let protoDecoder = createProtoDecoder(handleFrame);
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
    log("Connected.");
    await sendCommand("STATUS");
  } catch (err) {
    setStatus("connection failed", "error");
    log("Connection error: " + err.message);
  }
}

function onDisconnected() {
  setStatus("disconnected", "disconnected");
  showConnected(false);
  rxChar = null;
  protoDecoder = createProtoDecoder(handleFrame);
  log("Device disconnected.");
}

function onNotify(event) {
  const v = event.target.value;
  protoDecoder(new Uint8Array(v.buffer, v.byteOffset, v.byteLength));
}

function handleFrame(f) {
  if (f.type === "telemetry") renderTelemetry(f);
  else if (f.type === "knock") renderKnock(f);
  else if (f.type === "reply") log(`[${f.ok ? "OK" : "ERROR"}] ${f.message}`);
  else if (f.type === "status") renderStatus(f);
}

function renderNoImu() {
  ui.valTilt.textContent = "0.0";
  ui.valTiltTag.textContent = "No IMU";
  ui.valAccel.textContent = "Not connected (D4/D5)";
  ui.valMag.textContent = "0.00";
}

function renderMail(inside) {
  ui.valMailInside.textContent = inside ? "Yes" : "No";
}

function renderTelemetry(t) {
  ui.valVLive.textContent = (t.v_live / 1000).toFixed(2) + " V";

  const imuAttached = t.ax !== 0 || t.ay !== 0 || t.az !== 0 || t.mag !== 0;
  if (imuAttached) {
    ui.valTilt.textContent = t.pitch.toFixed(1);
    ui.valTiltTag.textContent = t.pitch.toFixed(1) + "°";
    ui.valAccel.textContent = `X:${t.ax.toFixed(2)} Y:${t.ay.toFixed(2)} Z:${t.az.toFixed(2)}`;
    ui.valMag.textContent = t.mag.toFixed(2);
  } else {
    renderNoImu();
  }

  ui.valKnocks.textContent = t.knocks;
  renderMail(t.mail_inside);
  ui.valTemp.textContent = t.temp;
  ui.valTempTag.textContent = t.temp + " °C";
}

// STATUS frames carry the slow-changing fields (one component per frame).
function renderStatus(s) {
  switch (s.name) {
    case "power":
      ui.valVRest.textContent = (s.v_rest / 1000).toFixed(2);
      ui.valVSag.textContent = s.v_sag;
      ui.valVLive.textContent = (s.v_live / 1000).toFixed(2) + " V";
      ui.valBatHealth.textContent = batteryHealth(s.v_rest);
      ui.valTemp.textContent = s.temp;
      ui.valTempTag.textContent = s.temp + " °C";
      break;
    case "imu":
      ui.valZero.textContent = s.zero.toFixed(1);
      if (!s.present) renderNoImu();
      break;
    case "lora":
      ui.valLoraSession.textContent = s.session;
      ui.valLoraTag.textContent = s.session;
      ui.valFCnt.textContent = s.fcnt;
      ui.valLastPayload.textContent = s.last_payload || "None";
      break;
    case "knock":
      ui.valKnocks.textContent = s.count;
      break;
    case "system":
      ui.firmwareVersion.textContent = "v" + s.fw;
      ui.valBench.textContent = s.bench ? "On" : "Off";
      ui.valUsb.textContent = s.usb ? "Connected" : "No";
      renderMail(s.mail_inside);
      ui.inputStream.value = s.stream_ms;
      break;
  }
}

function renderKnock(k) {
  ui.valKnocks.textContent = k.count;
  ui.valKnockPeak.textContent = k.mag.toFixed(2);
  ui.knockPulseIndicator.textContent = `Hit #${k.count} (${k.mag.toFixed(2)}g)`;
  ui.knockPulseIndicator.dataset.hit = "active";

  clearTimeout(knockTimer);
  knockTimer = setTimeout(() => {
    ui.knockPulseIndicator.dataset.hit = "idle";
    ui.knockPulseIndicator.textContent = "Ready";
  }, 600);

  log(`*** [KNOCK HIT] #${k.count} registered | Peak force: ${k.mag.toFixed(2)}g ***`);
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

ui.btnStatus.addEventListener("click", () => sendCommand("STATUS"));
ui.btnStream.addEventListener("click", () => {
  const ms = Number.parseInt(ui.inputStream.value, 10);
  if (!Number.isInteger(ms) || ms < 0) {
    log("Error: stream interval must be 0 (off) or milliseconds.");
    return;
  }
  sendCommand(`STREAM ${ms}`);
});

ui.btnDisconnect.addEventListener("click", async () => {
  await sendCommand("SLEEP");
  if (device && device.gatt.connected) device.gatt.disconnect();
});
