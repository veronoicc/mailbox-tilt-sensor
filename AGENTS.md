# Repository Guidelines

## Project Overview
Battery-powered LoRaWAN mailbox-flap tilt sensor. Seeed XIAO nRF52840 + Wio-SX1262 (EU868 default), Grove LIS3DHTR accelerometer (I²C `0x19`), CR123A cell. Sends OTAA uplinks to TTN; a static Web Bluetooth console (`web/`) provides local maintenance over BLE.

## Architecture & Data Flow
- **Firmware is a one-shot state machine, not a loop.** The LIS3DH INT1 pin (D0) wakes the MCU from System-Off, which is a full reboot. `setup()` runs once and ends in `sleepUntilWake()`. `loop()` is empty.
- `setup()` order: watchdog → LED blink → serial (debug env) → read `RESETREAS` → IMU init + battery read → optional BLE maintenance → knock check (`MailboxEmptied`) or flap sampling → `MailDelivered` uplink → sleep.
- No RTOS or ISRs. Use blocking `delay()` and `millis()` polling.
- **State** lives in file-scope `static g_*` globals in each module. They reset on every wake. The tilt zero offset is NOT persisted. LoRaWAN session/nonces live in `.noinit` RAM guarded by `SESSION_MAGIC` (`src/lora/lorawan.cpp`).
- **Uplink** (FPort 1, 7 bytes): `[0]` battery `(mV-2000)/10`, `[1]` sag/5, `[2]` peak tilt, `[3]` resting tilt, `[4]` open duration ×100 ms, `[5]` int8 temperature, `[6]` event (`01` delivered, `02` emptied, `03` manual test).
- **BLE** (Nordic UART, newline-delimited): browser→device `PIN:<pin> CMD:<ACTION>` with actions `RECALIBRATE`, `TEST_UPLINK`, `RESET_KNOCKS`, `DFU`, `SLEEP`. Device→browser single-line JSON: `{"type":"telemetry",...}`, `{"type":"knock",...}`, `{"status":"OK|ERROR","msg":...}`.

### Sync contracts (change both sides together)
| Change in | Must also update |
|---|---|
| `src/lora/uplink.{h,cpp}`: byte layout, scaling, FPort, event codes | `ttn/payload_formatter.js` (+ its test), `test/test_uplink`, README §4.4, and the user's TTN Console copy (pasted by hand, not deployed) |
| `src/ble/console.cpp`: BLE JSON fields, command names, line format | `web/app.js` |
| LoRa region | `LORAWAN_BAND` in `include/settings.h` |

## Key Directories
- `src/main.cpp`: wake flow only. `src/core/` (log, power/battery/watchdog), `src/motion/` (imu, knock, flap), `src/lora/` (lorawan, uplink, hex_key), `src/ble/` (console).
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
- PlatformIO, Arduino framework on the Adafruit nRF52 core (`bluefruit.h`). Envs: `release` (default, no serial), `debug` (serial @ 115200, `DEBUG_SERIAL=1`), `native` (host tests). Platform is pinned to a commit. Deps: `jgromes/RadioLib @ ^6.6.0`.
- `lib_archive = no` is set in `platformio.ini` (a common nRF52 setting so library weak symbols/ISRs link). Its necessity here is unverified; don't remove it without a build and on-device test.
- Credentials and `BLE_AUTH_PIN` live in the gitignored `include/secrets.h`. NEVER commit real keys.
- `FIRMWARE_VERSION` is defined in `src/version.h`.
- The web UI uses Web Bluetooth, so it needs HTTPS and a Chromium browser. It loads Google Fonts from a CDN.

## Testing & QA
- Native Unity tests (`pio test -e native`) cover `src/lora/uplink.cpp` and `src/lora/hex_key.cpp`; the formatter has `node --test ttn/payload_formatter.test.js`. The byte vectors in both mirror each other: keep them in sync. Everything else is verified with `pio run -e release` / `-e debug`; hardware behaviour needs a device.
- CI only deploys `web/`. A push to `main` touching `web/**` goes live immediately, so treat web edits as production.

## Known Gaps and Gotchas
- `include/secrets.h` is required and gitignored; copy `include/secrets.example.h` (the build errors with a clear message otherwise).
- Battery is read from `analogReadVDD()` (the CR123A sits on 3V3). The old `PIN_VBAT_ENABLE` path never compiled.
- RadioLib defaults ADR on, so `ENABLE_ADR false` is applied explicitly via `setADR(ENABLE_ADR)`.
- Every `ENABLE_*` toggle must compile in both states; gate bodies, not call sites.
- Hardware safety: never plug in USB while a CR123A is connected to 3V3.
