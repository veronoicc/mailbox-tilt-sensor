# Repository Guidelines

## Project Overview
Battery-powered LoRaWAN mailbox-flap tilt sensor. Seeed XIAO nRF52840 + Wio-SX1262 (EU868 default), Grove LIS3DHTR accelerometer (I²C `0x19`), CR123A cell. Sends OTAA uplinks to TTN; a static Web Bluetooth console (`web/`) provides local maintenance over BLE.

## Architecture & Data Flow
- **Two sleep modes, chosen by `HEARTBEAT_INTERVAL_HOURS`.** `0`: System-Off. The LIS3DH INT1 pin (D0) wakes the MCU with a full reboot, `setup()` handles one event and ends in `systemOff`, and `loop()` is empty. `>0` (default 24): System-On sleep so a heartbeat timer works. `setup()` runs once at boot, then `loop()` sleeps in `sleepWait()` (`src/core/sleep.cpp`, FreeRTOS tickless idle, INT1 via the framework's `attachInterrupt`) until INT1 fires or the heartbeat is due. No reboot on a flap wake in this mode.
- `setup()` order: watchdog → LED blink → serial (debug env) → read `RESETREAS` + startup counter → IMU init + battery read → startup heartbeat (`heartbeatOnStartup`) → awake window on a real startup, else `handleWake()` (knock check → `MailCleared`, flap hold → poll uplink + awake window, flap delivery → `MailInserted`) → sleep.
- No ISRs in app code except the INT1 callback in `sleep.cpp`. Use blocking `delay()` and `millis()` polling elsewhere.
- **State** lives in file-scope `static g_*` globals in each module (they reset on every wake in System-Off mode, persist in System-On mode). The tilt zero offset is NOT persisted. The LoRaWAN session lives in `.noinit` RAM guarded by `SESSION_MAGIC` (`src/lora/lorawan.cpp`). Flash (InternalFS, atomic tmp + rename via `src/core/store.cpp`): `/lw_nonces` (DevNonce, must never repeat for TTN, saved after every join attempt) and `/state` (mail inside or not, `src/core/state.cpp`). `src/core/startup.cpp` keeps a `.noinit` consecutive-startup counter for the rapid-reboot guard.
- **Uplink** (FPort 1, one fixed 8-byte frame, `src/lora/uplink.h`): `[0]` reason (`01` mail_inserted, `02` mail_cleared, `03` heartbeat, `04` heartbeat_requested, `05` heartbeat_startup, `06` heartbeat_poll, `07` manual_test), `[1]` battery `(mV-2000)/10`, `[2]` sag/5, `[3]` int8 temperature, `[4]` mail inside, `[5]` peak tilt, `[6]` resting tilt, `[7]` open duration ×100 ms. `lorawanSend()` fills battery, sag, temperature, mail state and (unless the flap data is set) the current tilt. **Downlink** (FPort 2): one opcode byte, `01` heartbeat, `02` clear.
- **Commands** live in one table, `src/cmd/cmd.cpp` (parser in `cmd_parse.cpp`, pure). BLE (`PIN:<pin> CMD:<ACTION> [args]`, Nordic UART, newline-delimited), the USB serial CLI (`src/cmd/serial_cli.cpp`, bare `action args`, no PIN, e.g. `status imu`) and TTN downlinks (opcode column) all run through it. Actions: `RECALIBRATE`, `TEST_UPLINK`, `RESET_KNOCKS`, `DFU`, `SLEEP`, `HELP`, `HEARTBEAT`, `CLEAR`, `STATUS [POWER|IMU|LORA|KNOCK|SYSTEM|ALL]`, `STREAM <ms>` (BLE only). Handlers never format output: they call `CmdOut.reply(ok, msg)` / `CmdOut.status(comp)` and each source formats it (BLE binary frames in `console.cpp`, serial text in `serial_cli.cpp`, debug log in `main.cpp`); status data is gathered once in `src/cmd/status.cpp`. Nothing prints periodically.
- **BLE protocol** (`src/ble/proto.{h,cpp}`, Arduino-free, native-tested; mirrored by `web/proto.js`): device→browser binary frames `0xA5, type, len(<=64), payload, crc8(poly 0x07, over type+len+payload)`, little-endian, frames may span notifications. Types: `01` TELEMETRY (streamed every `BLE_STREAM_DEFAULT_MS`, reset on each connection, `STREAM <ms>` changes it, `0` = off), `02` KNOCK, `03` REPLY (`ok` + <=60 B UTF-8), `04` STATUS (component 1 power, 2 imu, 3 lora, 4 knock, 5 system). Every state-changing command replies REPLY then the relevant STATUS. There is no JSON anywhere on the BLE link.
- **Awake window** (`bleConsoleRun()`): opens on a real startup or a flap hold; stays open while a BLE client is connected, closes `BLE_TIMEOUT_SECONDS` after opening or after the last client left. Downlinks are only received after an uplink (Class A); `cmdDrainDownlinks` runs after every send.

### Sync contracts (change both sides together)
| Change in | Must also update |
|---|---|
| `src/lora/uplink.{h,cpp}`: byte layout, scaling, FPort, message types, downlink opcodes | `ttn/payload_formatter.js` (+ its test), `test/test_uplink`, README §4.4, and the user's TTN Console copy (uplink AND downlink formatter, pasted by hand, not deployed) |
| `src/cmd/cmd.cpp`: command names, downlink opcodes; `src/ble/proto.{h,cpp}`: BLE frame layouts (+ `test/test_proto`) | `web/proto.js` (+ `web/proto.test.js`, same byte vectors), `web/app.js`, `ttn/payload_formatter.js` (opcode map), README §5.2/§5.3 |
| LoRa region | `LORAWAN_BAND` in `include/settings.h` |

## Key Directories
- `src/main.cpp`: wake flow only. `src/core/` (log, power/battery/watchdog, sleep, store, state, startup, heartbeat_policy), `src/motion/` (imu, knock, flap), `src/lora/` (lorawan, uplink, hex_key), `src/cmd/` (command table, parser, status snapshot, serial CLI), `src/ble/` (console / awake window, binary protocol).
- `include/settings.h`: user knobs and feature toggles. `include/board.h`: fixed pins. `include/secrets.h`: credentials (gitignored).
- `ttn/`: payload formatter and its Node test. `test/`: native Unity suites.
- `web/`: static console. `index.html` loads `app.js`. `app.css` imports `tokens.css`, then `base.css`.
- `.github/workflows/deploy-pages.yml`: publishes `web/` to GitHub Pages.

## Development Commands
```
pio run                  # release build (default env)
pio run -e debug -t upload   # flash with serial logging (double-tap reset for UF2 bootloader if port is missing)
pio device monitor       # serial, 115200
pio test -e native       # host unit tests
```
No lint or format tooling is configured. The first build needs network (git platform, RadioLib, native platform/Unity).

## Code Conventions & Common Patterns
- Two-space indent, `//` comments. Exported functions are `camelCase` with a module prefix (`imuRead`, `lorawanSendEvent`), file-private helpers `static camelCase`, globals `static g_*`, macros and constants `UPPER_SNAKE`. Includes are src-rooted (`"motion/imu.h"`).
- Feature gates are `#if ENABLE_*`, kept inside the module where possible. Debug output uses the `DBG_PRINTF`/`DBG_PRINTLN` macros from `core/log.h` (no `#if` needed) with bracketed tags like `[BLE]`, `[KNOCK]`, `[LoRaWAN TX]`.
- Error handling: functions return `bool` or an error code, with no exceptions. Callers mostly continue silently. The NRF WDT (30 s) is fed in the send and BLE loops.
- If `WHO_AM_I != 0x33` the IMU is missing. Firmware enters bench mode and must NOT call `systemOff`, because the device would be unwakeable. USB presence also blocks System-Off.
- Web CSS: use `var(--token)` from `tokens.css` (oklch colours, `--space-*`, `--text-*`), not hard-coded values. UI state is set via `data-*` attributes and the `hidden` attribute.
- Formatter JS is ES5 style (`var`), a single `decodeUplink(input)`.

## Important Files
`src/main.cpp`, `include/settings.h`, `include/board.h`, `include/secrets.example.h`, `platformio.ini`, `ttn/payload_formatter.js`, `web/index.html` + `web/app.js`, `README.md` (BOM, wiring, TTN setup).

## Runtime/Tooling Preferences
- PlatformIO, Arduino framework on the Adafruit nRF52 core (`bluefruit.h`). Envs: `release` (default, no serial), `debug` (serial @ 115200, `DEBUG_SERIAL=1`, plus `RADIOLIB_DEBUG_BASIC/PROTOCOL` protocol logging), `native` (host tests). Platform is pinned to a commit. Deps: `jgromes/RadioLib @ ^6.6.0`.
- `lib_archive = no` is set in `platformio.ini` (a common nRF52 setting so library weak symbols/ISRs link). Its necessity here is unverified; don't remove it without a build and on-device test.
- Credentials and `BLE_AUTH_PIN` live in the gitignored `include/secrets.h`. NEVER commit real keys.
- `FIRMWARE_VERSION` is defined in `src/version.h`.
- The web UI uses Web Bluetooth, so it needs HTTPS and a Chromium browser. It loads Google Fonts from a CDN.

## Testing & QA
- Native Unity tests (`pio test -e native`) cover `src/lora/uplink.cpp`, `src/lora/hex_key.cpp`, `src/cmd/cmd_parse.cpp`, `src/core/heartbeat_policy.cpp` and `src/ble/proto.cpp`; the formatter has `node --test ttn/payload_formatter.test.js` and the web protocol has `node --test web/proto.test.js`. The byte vectors in `test_uplink`/formatter and in `test_proto`/`proto.test.js` mirror each other: keep them in sync. Everything else is verified with `pio run -e release` / `-e debug`; hardware behaviour needs a device.
- CI only deploys `web/`. A push to `main` touching `web/**` goes live immediately, so treat web edits as production.

## Known Gaps and Gotchas
- `include/secrets.h` is required and gitignored; copy `include/secrets.example.h` (the build errors with a clear message otherwise).
- Battery is read from `analogReadVDD()` (the CR123A sits on 3V3). The old `PIN_VBAT_ENABLE` path never compiled.
- RadioLib defaults ADR on, so `ENABLE_ADR false` is applied explicitly via `setADR(ENABLE_ADR)`.
- Every `ENABLE_*` toggle must compile in both states; gate bodies, not call sites.
- Hardware safety: never plug in USB while a CR123A is connected to 3V3.
