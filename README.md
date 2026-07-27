# ESP8266 IEC 104 Emulator

Emulates an IEC 60870-5-104 outstation on an ESP8266. Exposes one controllable
point (mirrored to 4 GPIOs for LEDs) and one measured point (mains frequency),
controllable/viewable both over IEC104 and a built-in web UI. Also acts as its
own WiFi access point for initial/remote network configuration.

## Hardware

Board: ESP8266 D1 Mini (or any NodeMCU-pinout ESP8266 board).

| Signal            | GPIO | D1 Mini pin | Notes |
|-------------------|------|-------------|-------|
| Onboard LED       | 2    | D4          | Active-low (built-in blue LED) |
| External LED 1    | 12   | D6          | Active-high |
| External LED 2    | 13   | D7          | Active-high |
| External LED 3    | 14   | D5          | Active-high |

All four LEDs mirror the same single logical point — they turn on/off
together (see `PINCTRL_setLed()` in `pinctrl.cpp`). Pin numbers are defined in
`config.h`.

`FREQ_PIN` (GPIO5/D1) reads mains frequency from a zero-crossing pulse — one
edge per AC cycle expected (e.g. from an opto-isolated zero-cross detector;
see `freqmeter.cpp`). Not galvanically isolated on its own — never wire this
pin directly to mains without isolation (optocoupler/transformer) between it
and the AC side.

Frequency is measured as a single raw cycle period (no averaging), so it
carries interrupt-jitter noise (roughly ±0.05-0.2Hz per reading on ESP8266
under WiFi activity). This is a demo of the IEC104 point, not a precision
instrument.

## Functionality

### WiFi

- Boots as an open access point, SSID `IEC104_<MAC>` (`AP_NAME_PREFIX` in
  `config.h`), IP `192.168.1.1`.
- If a saved station SSID is in range, it joins that network — in dual AP+STA
  mode, so the access point and web UI stay reachable throughout.
- The saved SSID/password are stored in EEPROM.

### Web UI (port 80)

- `/` — home page:
  - LED toggle: green circle = on (click to turn off), gray = off (click to
    turn on). Polls actual state every 1s, so an IEC104 command is reflected
    live.
  - Mains frequency, refreshed every 2s; blank if no valid signal.
  - Shows the station IP once connected to an external network.
- `/selectap` — scans nearby WiFi networks and lets you save a new SSID/password
  (device restarts to apply).

### IEC 60870-5-104 server (port 2404, `IEC104_PORT`)

One TCP master connection at a time. Handles STARTDT/STOPDT/TESTFR
(U-format) and General Interrogation. Points (common address 1,
`IEC104_COMMON_ADDR`):

| IOA  | Type       | Description |
|------|------------|-------------|
| 1001 | M_SP_NA_1  | LED state (spontaneous on change, included in GI) |
| 2001 | C_SC_NA_1  | LED command (turns the LEDs on/off) |
| 1002 | M_ME_NC_1  | Mains frequency, streamed every 2s, included in GI |

IOA/port/common-address values are all in `config.h`.

## Build / upload

```
tools/install_dependencies.sh   # arduino-cli + esp8266 core + libraries
tools/build.sh                  # compile
tools/upload_usb.sh             # build, then flash over USB (/dev/ttyUSB0)
```
