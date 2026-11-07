# Repository Guidelines

## Project Overview
Battery-powered LoRaWAN mailbox-flap tilt sensor. Seeed XIAO nRF52840 + Wio-SX1262 (EU868 default), Grove LIS3DHTR accelerometer (I²C `0x19`), CR123A cell. Sends OTAA uplinks to TTN; a static Web Bluetooth console (`web/`) provides local maintenance over BLE.

## Architecture & Data Flow
- **Firmware is a one-shot state machine, not a loop.** The LIS3DH INT1 pin (D0) wakes the MCU from System-Off, which is a full reboot. `setup()` runs once and ends in `enterDeepSleep()`. `loop()` is empty.
- `setup()` order: watchdog → LED blink → serial (if USB) → read `RESETREAS` → IMU init + battery read → optional BLE maintenance → flap sampling → `EVENT_MAIL_DELIVERED` uplink (or knock check → `EVENT_MAILBOX_EMPTIED`) → sleep.
- No RTOS or ISRs. Use blocking `delay()` and `millis()` polling.
- **State** lives in file-scope `static g_*` globals in `src/main.cpp`. They reset on every wake. The tilt zero offset is NOT persisted. LoRaWAN session/nonces live in `.noinit` RAM guarded by `SESSION_MAGIC`.
- **Uplink** (FPort 1, 7 bytes): `[0]` battery `(mV-2000)/10`, `[1]` sag/5, `[2]` peak tilt, `[3]` resting tilt, `[4]` open duration ×100 ms, `[5]` int8 temperature, `[6]` event (`01` delivered, `02` emptied, `03` manual test).
- **BLE** (Nordic UART, newline-delimited): browser→device `PIN:<pin> CMD:<ACTION>` with actions `RECALIBRATE`, `TEST_UPLINK`, `RESET_KNOCKS`, `DFU`, `SLEEP`. Device→browser single-line JSON: `{"type":"telemetry",...}`, `{"type":"knock",...}`, `{"status":"OK|ERROR","msg":...}`.

### Sync contracts (change both sides together)
| Change in `src/main.cpp` | Must also update |
|---|---|
| Uplink byte layout, scaling, FPort, event codes | `ttn_payload_formatter.js`, README §4.4, and the user's TTN Console copy (pasted by hand, not deployed) |
| BLE JSON fields, command names, line format | `web/index.html` inline script |
| LoRa region | `LORAWAN_BAND` in `include/settings.h` AND the `-D ..._REGION_EU868` flags in `platformio.ini` |

## Key Directories
- `src/main.cpp`: all firmware (about 830 lines, single file).
- `include/settings.h`: user knobs, credentials, feature toggles.
- `include/config.h`: fixed board pins, wrapped in `#ifndef`.
- `web/`: static console. `index.html` has inline JS. `app.css` imports `tokens.css`, then `base.css`.
- `.github/workflows/deploy-pages.yml`: publishes `web/` to GitHub Pages.
- `lib/`, `test/`, `include/README`: PlatformIO boilerplate only.

## Development Commands
```
pio run                  # build (the only verification available)
pio run -t upload        # flash (double-tap reset for UF2 bootloader if port is missing)
pio device monitor       # serial, 115200
pio run -t clean
```
No lint or format tooling is configured. The first build needs network (unpinned git platform, RadioLib download).

## Code Conventions & Common Patterns
- Two-space indent, `//` comments. Helpers are `static camelCase`, globals `g_` prefixed, macros and constants `UPPER_SNAKE`.
- Feature gates are `#if ENABLE_*`. Debug output uses the `DBG_PRINT*` macros (gated by `DEBUG_SERIAL`) with bracketed tags like `[BLE]`, `[KNOCK]`, `[LoRaWAN TX]`.
- Error handling: functions return `bool` or an error code, with no exceptions. Callers mostly continue silently. The NRF WDT (30 s) is fed in the send and BLE loops.
- If `WHO_AM_I != 0x33` the IMU is missing. Firmware enters bench mode and must NOT call `systemOff`, because the device would be unwakeable. USB presence also blocks System-Off.
- Web CSS: use `var(--token)` from `tokens.css` (oklch colours, `--space-*`, `--text-*`), not hard-coded values. UI state is set via `data-*` attributes and the `hidden` attribute.
- Formatter JS is ES5 style (`var`), a single `decodeUplink(input)`.

## Important Files
`src/main.cpp`, `include/settings.h`, `include/config.h`, `platformio.ini`, `ttn_payload_formatter.js`, `web/index.html`, `README.md` (BOM, wiring, TTN setup).

## Runtime/Tooling Preferences
- PlatformIO, Arduino framework on the Adafruit nRF52 core (`bluefruit.h`). Single env `seeed_xiao_nrf52840`. Deps: `jgromes/RadioLib @ ^6.6.0`.
- `lib_archive = no` is set in `platformio.ini` (a common nRF52 setting so library weak symbols/ISRs link). Its necessity here is unverified; don't remove it without a build and on-device test.
- `include/settings.h` is tracked in git and holds placeholder TTN keys and `BLE_AUTH_PIN "123456"`. NEVER commit real keys.
- `FIRMWARE_VERSION` is defined in `main.cpp`, not in `settings.h`.
- The web UI uses Web Bluetooth, so it needs HTTPS and a Chromium browser. It loads Google Fonts from a CDN.

## Testing & QA
- There are no tests, no native env and no firmware CI. Verify firmware with `pio run`. Hardware behaviour needs a device.
- CI only deploys `web/`. A push to `main` touching `web/**` goes live immediately, so treat web edits as production.
- If you add tests, the formatter is the easiest target (Node). Firmware tests would need logic split out of `main.cpp` plus an `[env:native]`.

## Known Gaps and Gotchas
- `ENABLE_HEARTBEAT`, `HEARTBEAT_INTERVAL_HOURS`, `JAMMED_THRESHOLD_DEG` and `DEBOUNCE_COOLDOWN_SEC` are defined but unused in `main.cpp`. The README's FPort 2 heartbeat is not implemented in firmware, though the formatter decodes it.
- `ttn_payload_formatter.js` maps every event code except `0x02` (including `0x03` test) to `mail_delivered`.
- The README's `mail_volume` JSON example is stale. The formatter emits snake_case enums.
- `g_sessionMagic` is used in debug output without an `ENABLE_SESSION_PERSISTENCE` guard, so that combination fails to compile.
- The payload-building block is copy-pasted three times in `main.cpp`. Change all three together.
- Hardware safety: never plug in USB while a CR123A is connected to 3V3.
