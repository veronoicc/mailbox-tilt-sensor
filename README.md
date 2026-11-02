# LoRaWAN Mailbox Tilt Sensor

A battery-powered LoRaWAN sensor node for shared metal mailbox banks (*Briefkastenanlagen*). It mounts on the moving mail flap, measures the flap opening angle and duration, and uses accelerometer reference register latching to prevent infinite wake loops when mail wedges the flap open.

Built on an **nRF52840 + SX1262** hardware stack powered directly from a **3.0V CR123A Lithium** cell, maintaining a **4 to 8 µA sleep current** down to sub-zero temperatures.

---

## 1. Hardware Requirements

### Component Bill of Materials

| Component | Specification | Function |
| :--- | :--- | :--- |
| **MCU & LoRa Transceiver** | **Seeed XIAO nRF52840 + Wio-SX1262 Kit** (EU868) | 64 MHz Cortex-M4F, ~2 µA deep sleep floor, Semtech SX1262 LoRa radio, U.FL connector with 868 MHz antenna. |
| **Tilt / Motion Sensor** | **LIS3DH / LIS3DHTR** 3-Axis MEMS Breakout (e.g. Seeed Grove or generic) | Measures flap opening angle (0°–90°), generates hardware motion interrupt on INT1 pin (~2 µA @ 1 Hz, 6 µA @ 10 Hz). |
| **Battery** | **1x CR123A Lithium (Li-MnO₂)** (3.0V nominal, ~1500 mAh) | Direct 3.0V supply rail (no LDO regulator losses). Rated from -40 °C to +70 °C with low self-discharge (<1%/year). |
| **Pulse Buffer Capacitor** | **100 µF 16V or 25V Radial Electrolytic** (Low ESR) | Absorbs ~100–118 mA SX1262 LoRa transmit burst when cold temperatures increase battery internal resistance. |
| **Flexible Hinge Harness** | **5-Conductor Ribbon Cable** (or 30 AWG / 0.14 mm² flexible stranded wire) | Flexible service loop across the mailbox flap hinge (3V3, GND, SDA, SCL, INT1). |
| **Chassis Mounting** | **2–3x Neodymium Disc Magnets** (Ø 8×2 mm or 10×2 mm, N35–N52) | Press-fit into the main 3D-printed enclosure to attach securely to the static sheet-steel mailbox interior. |
| **Flap Mounting** | **Double-Sided Outdoor Acrylic Foam Tape** (e.g. 3M VHB or Tesa) | Secures the lightweight moving sensor pod to the inside of the flap. |
| **Enclosure** | **3D-Printed Chassis & Flap Pod** | Printed in **PETG or ASA** (resistant to sub-zero embrittlement and mechanical impact). |

---

## 2. Mechanical Design & 3D Printing

Metal mailboxes act as a **Faraday cage** and subject internal electronics to heavy mechanical shock from incoming catalogs and magazines. This build uses a **Split-Mount Configuration**:

```
+-----------------------------------------------------------------------+
| MAILBOX CROSS SECTION (Hinged Flap Operation)                         |
|                                                                       |
| Outer Metal Front Wall                                                |
|       |                                                               |
|       | Hinge                                                         |
|       |   O <--- Service Loop (5x Flexible Ribbon / Stranded Wires)   |
|       |  / \                                                          |
|       | /   \                                                         |
|       |/     +----------------------------------+                     |
|  [FLAP POD]  | STATIC MAIN UNIT                 |                     |
|  (LIS3DHTR)  | - XIAO nRF52840 + Wio-SX1262     |                     |
|  Chamfered   | - CR123A Battery Cradle          |                     |
|  Wedge <10mm | - 100 µF Buffer Cap              |                     |
|       |      | - Sunk Neodymium Magnets         |                     |
|       |      +----------------------------------+                     |
|       v                      |                                        |
|  Mail Slot (Aperture) <------+ Antenna positioned 8mm from slit edge  |
|                                                                       |
+-----------------------------------------------------------------------+
```

### 2.1 The Two Enclosures
1. **Moving Flap Pod (Sensor Pod):**
   * Houses only the LIS3DH breakout board.
   * Design as a **low-profile 45-degree chamfered wedge** (<10 mm total height). Incoming heavy mail glides over the sloping face without snagging or shearing the sensor off the flap.
   * Attached to the inside of the flap using double-sided acrylic foam tape.
2. **Static Main Unit (Chassis):**
   * Houses the XIAO nRF52840, Wio-SX1262 board, buffer capacitor, and CR123A battery.
   * Mounts to the static inner roof or side partition of the mailbox using **2 or 3 neodymium disc magnets** sunk into the print.
   * **Integrated Battery Cradle:** Model a 17.2 mm diameter semi-cylindrical pocket with two vertical slots at the ends. Bend two tinned wire leads through the slots as contact terminals. The natural flex of the printed plastic wall acts as the spring contact (eliminating the need for a separate commercial battery holder).

### 2.2 Slicing & Filament
* **Filament Material:** **PETG or ASA** (Do **NOT** use standard PLA: PLA becomes brittle below 5 °C and cracks under repeated flap slamming).
* **Walls & Infill:** 3 perimeters, 25% infill (Gyroid or Grid).

### 2.3 The RF Slot-Antenna Technique
Metal mailboxes block RF signals from escaping through sheet metal. However, the horizontal mail slot functions as a **slot antenna aperture**:
* Mount the flexible 868 MHz FPC antenna flat along the inside front lip of the mailbox, directly along the mail slot opening.
* Keep the radiating element spaced **at least 6 to 10 mm away from bare metal** using a 3D-printed standoff to prevent metal detuning.

---

## 3. Electrical Wiring Diagram

```
                                      +------------------------------------+
                                      |      VARTA CR123A (3.0V Nominal)   |
                                      +--------(+)------------------(-)----+
                                                |                    |
                                                |                    |
                                         +------+------+      +------+------+
                                         | 100 µF 16V  |      |             |
                                         | Buffer Cap  |      |             |
                                         +------+------+      |             |
                                                |             |             |
     +------------------------------------------+             |             |
     |                                                        |             |
     |                     +----------------------------------+             |
     |                     |                                                |
     v                     v                                                |
+-----------------------------------+                                       |
| SEEED XIAO nRF52840               |                                       |
|                                   |                                       |
| [3V3 Pin] <-----------------------+                                       |
| [GND Pin] <---------------------------------------------------------------+
|                                   |
| [D4 / SDA] -----------------------+-------------------+
| [D5 / SCL] -----------------------|-----------------+ |
| [D0 / INT1] ----------------------|---------------+ | |
|                                   |               | | |
| (Internal Wio-SX1262 Sandwich)    |               | | |
| SPI: D8(SCK), D9(MISO), D10(MOSI) |               | | |
| Control: D1(CS), D2(RST), D3(BUSY)|               | | |
+-----------------+-----------------+               | | |
                  |                                 | | |
                  v                                 | | |
+-----------------------------------+               | | |
| Wio-SX1262 LoRa Expansion Board   |               | | |
| - Semtech SX1262 Radio            |               | | |
| - U.FL 868 MHz Antenna            |               | | |
+-----------------------------------+               | | |
                                                    | | |
              5-Wire Flexible Hinge Service Loop    | | |
====================================================|=|=|=============
                                                    | | |
                                                    v v v
                                    +-----------------------------------+
                                    | LIS3DH / LIS3DHTR ACCELEROMETER   |
                                    |                                   |
                                    | [VCC / 3V3 Pin] <-----------------+ (From 3V3 Rail)
                                    | [GND Pin] <-----------------------+ (From GND Rail)
                                    | [SDA Pin] <-----------------------+ (From D4)
                                    | [SCL Pin] <-----------------------+ (From D5)
                                    | [INT1 Pin] <----------------------+ (From D0)
                                    +-----------------------------------+
```

### Pin Interconnection Table

| XIAO nRF52840 Pin | Hardware Function | Connection Destination | Location |
| :---: | :---: | :---: | :---: |
| **3V3** | Direct 3.0V Supply Rail | Battery (+) & Sensor VCC | Main Chassis |
| **GND** | System Ground | Battery (-) & Sensor GND | Main Chassis |
| **D4** | I2C SDA (P0.04) | LIS3DH SDA | Across Hinge |
| **D5** | I2C SCL (P0.05) | LIS3DH SCL | Across Hinge |
| **D0** | Wake Interrupt (P0.02) | LIS3DH INT1 | Across Hinge |
| **D1, D2, D3, D6, D8-D10** | LoRa SPI & Control | Wio-SX1262 Board | Internal Pin Headers |

### Critical Wiring Rules
1. **Direct 3V3 Rail (Bypass LDO):** Connect the 3.0V battery directly to the **3V3 pin** (not VIN/5V). The nRF52840, SX1262, and LIS3DH operate directly down to 1.8V. Bypassing the onboard regulator eliminates quiescent current waste and LDO dropout voltage.
2. **Buffer Capacitor Polarity:** Solder the **100 µF capacitor** directly across 3V3 and GND terminals. Mind polarity: negative stripe (`-`) to GND, long lead to 3V3.
3. **Flashing Safety Warning:** **NEVER plug in a USB-C cable while a non-rechargeable CR123A battery is connected to the 3V3 rail.** The USB port feeds 5V into the board, which can damage a non-rechargeable primary cell. Disconnect the battery before plugging in USB!

---

## 4. The Things Network (TTN v3) Setup

This project uses **LoRaWAN 1.0.4 OTAA (Over-The-Air Activation)** on the **EU868** band.

### 4.1 Registering the Device in The Things Stack Console
1. Log in to your [The Things Network Console](https://console.cloud.thethings.network/) (select your cluster, e.g. Europe 1 / `eu1`).
2. Navigate to **Applications** and select (or create) your application.
3. Click **+ Register end device**.
4. Choose **Enter end device specifics manually**:
   * **Frequency plan:** `Europe 863-870 MHz (SF9 for RX2 - recommended)`
   * **LoRaWAN version:** `LoRaWAN Specification 1.0.4`
   * **Regional Parameters version:** `RP002-1.0.3`
5. Fill in the **Join settings**:
   * **JoinEUI (AppEUI):** Enter `0000000000000000` (16 zeros).
   * **DevEUI:** Click the generate button (or generate a random 16-hex character ID).
   * **AppKey:** Click the generate button (generates a 32-hex character secret key).
   * **End device ID:** Give it a clean name (e.g. `mailbox-sensor-1`).
6. Click **Register end device**.

### 4.2 Applying Credentials & Region to Firmware
All user configuration is isolated in a single dedicated file: [`include/settings.h`](./include/settings.h).

Simply paste the HEX strings directly from the TTN Console (spaces, dashes, colons, or "0x" prefixes are handled automatically):

```cpp
// 1. Regional Frequency Band (EU868, US915, AU915, AS923, KR920, IN865)
#define LORAWAN_BAND       EU868

// 2. TTN Credentials (Direct copy-paste from TTN Console)
#define TTN_JOIN_EUI_HEX   "0000000000000000"
#define TTN_DEV_EUI_HEX    "70B3D57ED0060000"
#define TTN_APP_KEY_HEX    "00112233445566778899AABBCCDDEEFF"
```

### 4.3 Setting Up the Payload Formatter
1. In the TTN Console under your Application, navigate to **Payload formatters -> Uplink**.
2. Set **Formatter type** to **Custom Javascript formatter**.
3. Copy the entire contents of [`ttn_payload_formatter.js`](./ttn_payload_formatter.js) from this repository and paste it into the editor.
4. Click **Save changes**.

### 4.4 LoRaWAN Uplink Payload Specification

#### FPort 1: Mail Activity & Knock Events (7 Bytes)
Transmitted whenever the flap opens or 4 knocks are detected.

| Byte | Field | Type | Scale / Range | Description |
| :---: | :--- | :---: | :--- | :--- |
| **0** | `battery_rest_mv` | `uint8` | `(mV - 2000) / 10` | Resting battery voltage (2000–4550 mV). |
| **1** | `battery_sag_mv` | `uint8` | `sag_mv / 5` | Battery voltage drop under 100 mA LoRa TX load (ESR health check). |
| **2** | `peak_tilt_deg` | `uint8` | 0°–90° | Maximum opening angle recorded during flap movement. |
| **3** | `resting_tilt_deg`| `uint8` | 0°–90° | Resting angle after settling. **> 10° indicates flap jammed open!** |
| **4** | `open_duration_s` | `uint8` | `0.1s steps` | Total time flap spent open (0–25.5 seconds). |
| **5** | `temperature_c` | `int8` | -40 °C to +85 °C | Internal mailbox temperature from on-die sensor. |
| **6** | `event_type` | `uint8` | `0x01` or `0x02` | `0x01`: Mail Delivered (Flap Opened), `0x02`: Mailbox Emptied (4 Knocks). |

#### FPort 2: 24-Hour Keepalive / Heartbeat (5 Bytes)
Transmitted periodically to verify node health and battery status.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `battery_rest_mv` | `uint8` | Resting battery voltage: `(mV - 2000) / 10`. |
| **1** | `battery_sag_mv` | `uint8` | Voltage sag under load in 5 mV steps. |
| **2** | `resting_tilt_deg`| `uint8` | Current resting tilt (0° = closed, >10° = jammed). |
| **3** | `temperature_c` | `int8` | Internal temperature in °C. |
| **4** | `heartbeat_count` | `uint8` | 8-bit rolling heartbeat counter (0–255). |

Decoded JSON output example:
```json
{
  "event": "mail_delivered",
  "event_code": 1,
  "battery_voltage": 3.01,
  "battery_sag_mv": 45,
  "peak_tilt_deg": 48,
  "resting_tilt_deg": 0,
  "open_duration_s": 0.8,
  "temperature_c": 18,
  "is_jammed": false,
  "mail_volume": "Large Magazine / Catalog"
}
```

---

## 5. WebBluetooth Diagnostics & Maintenance Interface

To diagnose the sensor, re-calibrate zero, or trigger firmware updates without dismounting the enclosure, a dedicated WebBluetooth instrument console is included in [`web/`](./web/).

* **Online GitHub Pages App:** `https://veronoicc.github.io/mailbox-tilt-sensor/`
* *(Web Bluetooth requires a Secure Context (HTTPS), provided automatically by GitHub Pages).*

### 5.1 Triggering Maintenance Mode
1. Open the mailbox flap all the way (**> 60°**) and hold it open continuously for **15 seconds**.
   *(Normal mail drops and catalogs wedging the flap at 20°–40° will NOT trigger BLE mode, protecting the battery).*
2. The node wakes Bluetooth LE advertising as **`Mailbox-Sensor`** for 60 seconds.
3. Open the web app in Chrome / Edge (Android, PC, Mac) or Bluefy (iOS) and click **connect to mailbox sensor**.

### 5.2 PIN Authentication & Actions
All modifying actions are protected by a configurable PIN (defined in `settings.h`, default: `123456`):
* **Comprehensive Live Telemetry:** Streams resting voltage, live voltage, voltage sag under LoRa load, core temperature, calibrated pitch angle, raw 3-axis accelerometer values ($X, Y, Z$), vector magnitude ($g$), zero horizon offset, live knock counts, LoRaWAN session status, uplink frame counter (`FCntUp`), and last transmitted hex payload.
* **Knock Pattern Tracker:** Live hit indicator pulses on physical taps, displaying real-time hit counts and peak force ($g$) within the 2200 ms detection window.
* **Test LoRaWAN Uplink:** Transmits an on-demand test packet directly from the browser to verify gateway reception, returning the frame counter, hex payload, and live battery sag.
* **Zero Calibration:** Latches the current physical position as the new resting horizon ($0^\circ$).
* **Reset Knocks:** Clears the knock tracker counter.
* **Reboot to DFU:** Reboots the nRF52840 into the UF2 bootloader for USB firmware updates.
* **Inactivity Auto-Sleep:** After 60 seconds of no Bluetooth traffic, the BLE radio completely shuts off and the node drops back to 2 µA System-Off sleep.
### 5.3 Automated GitHub Pages Deployment (No Extra Branch Needed)
The web console in `web/` is automatically deployed via GitHub Actions:

1. In your GitHub repository at `veronoicc/mailbox-tilt-sensor`, go to **Settings -> Pages**.
2. Under **Build and deployment -> Source**, select **GitHub Actions**.
3. Every time you push to `main`, the included workflow ([`.github/workflows/deploy-pages.yml`](./.github/workflows/deploy-pages.yml)) automatically publishes the contents of `web/` to:
   `https://veronoicc.github.io/mailbox-tilt-sensor/`

## 6. Home Assistant Integration

You do **not** need to maintain manual MQTT configuration files. Home Assistant features an official, native **The Things Network** integration:

1. In Home Assistant, go to **Settings -> Devices & Services -> Add Integration**.
2. Search for **The Things Network**.
3. Follow the standard setup wizard to connect your TTN account/cluster.
4. Your mailbox node and its decoded telemetry (`battery_voltage`, `peak_tilt_deg`, `event`, `is_jammed`, etc.) will automatically be discovered as standard Home Assistant entities.

---

## 7. Power & Battery Minimization Architecture

To achieve 10+ years of operational runtime on a single CR123A battery, the firmware implements several aggressive hardware and algorithmic power-saving safeguards:

1. **Dual Hardware DC-DC Buck Converters:**
   * **nRF52840 DC-DC (`DCDCEN = 1`):** Reduces CPU and memory active current draw from ~11 mA down to ~5 mA.
   * **SX1262 DC-DC (`setRegulatorDCDC()`):** Cuts LoRa active TX and RX current consumption by ~40% compared to linear LDO mode.
2. **Early Settling Breakout:**
   * Standard mailbox drops complete in 400–800 ms. Once the flap returns to rest ($< 10^\circ$) and remains motionless for 400 ms, the sampling loop breaks immediately, eliminating up to 2200 ms of active CPU runtime per delivery.
3. **Rapid Knock Abort:**
   * When a vibration wakes the accelerometer, if a 2nd knock does not arrive within 500 ms, the CPU aborts and returns to deep sleep in under 20 ms rather than burning energy on stray noises.
4. **Zero Quiescent Peripheral Leakage:**
   * All onboard RGB LEDs are tri-stated (`pinMode(INPUT)`).
   * The battery ADC voltage divider is disconnected immediately after a 1 ms sample.
   * The BLE radio and SoftDevice stack are completely uninitialized during deep sleep and normal mail deliveries.

---

## 8. Firmware Compilation & Flashing

Firmware is built using PlatformIO.

```bash
# Clone and enter directory
cd ~/Projects/smart/mailbox-tilt-sensor

# Verify build
pio run

# Flash to device via USB-C (ensure battery is disconnected!)
pio run -t upload
```

*(Note: If the upload port is not automatically detected, double-tap the tiny reset button next to the XIAO USB-C port to force UF2 bootloader mode).*
