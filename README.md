This is a different TeHyBug firmware fully written in C/C++, (previous was partially in Lua).

This firmware supports easy OTA Updates.

<img src="images/2022-06-10T21_41_23.878Z-IMG_3707.jpg" width="500">

TeHyBug 18650 Universal


<img src="images/mini-tehybug.jpg" width="500">

Mini TeHyBug (ESP8285 — uses the `esp8285` firmware build, same as the universal)


This firmware is compatible with tehybug universal boards (without display) like:
* TeHyBug 18650 Universal v1 (esp-01 based) and v2 (esp-m based)
* Mini TeHyBug
* Gumboard 
* or other TeHyBug boards with have audio jack connector for the sensors
* It is also compatible with any other ESP8266/ESP8285 dev boards like wemos, lolin, nodemcu etc. See the pin mapping images. Only the indicator led will not work and the power saving mode with deep sleep will probably not work either.

## Buttons
- Reset: forces TeHyBug to reboot/restart
- Mode button: activates the configuration mode. Press it **after** the device has booted, not while pressing RESET — the MODE button is on GPIO0, so holding it down during reset puts the ESP into firmware-flash (UART download) mode instead.

## Device Modes
- Live mode: when your device is configured to serve data (via http/mqtt) and you enable the powersaving deep sleep and deactivate the config mode in the system settings. <img width="402" alt="Bildschirmfoto 2023-11-04 um 16 26 51" src="https://github.com/gumslone/tehybug/assets/12110353/2b2524da-0643-447a-abb0-873b50236c4e">

- Config mode: TeHyBug serves a web interface at http://tehybug.local where you can configure everything.

- Offline mode (requires the RTC + EEPROM module): the device never connects to WiFi. It wakes on the log interval, measures, appends one entry to the on-device log and deep-sleeps again — the lowest possible power draw with no network. See [Offline data logging](#offline-data-logging-rtc--eeprom) below.


To return back to Config mode from the Live mode (or Offline mode):
1. hit the RESET button and release it — do **not** hold MODE yet (holding MODE during reset boots the ESP into flash mode)
2. right after the device boots, push and hold the MODE button untill the LED turns blue
3. release the MODE button.

In Offline mode you have a 1-second window right after each boot to press MODE; in the other modes press it as the device boots.

## Offline data logging (RTC + EEPROM)

With a DS3231 RTC + I²C EEPROM module attached, TeHyBug can store timestamped readings on the device itself — no server, broker or network required. Configure and read the log on the **Data Log** page of the web interface.

- **One file per day, a full month retained.** A file per day of month is written. The 32 KB EEPROM (FT24C256A) is split into 32 slots of ~1 KB each, so every day of the month gets its own file; when no free slot is left the oldest day file is recycled.
- **Pick what to log.** Store the default measured set, or a custom placeholder template (e.g. `%temp% %humi%`) to keep only the fields you care about.
- **Compact format.** To fit more into the small slots (~1 KB each) the date is omitted — it is implied by the file name — and each value is tagged with a short code, e.g. `07:55 22.6t 48.3h 1013.2p`. This roughly doubles the entries per day file versus a verbose `key=value` line.
- **Own log interval.** The log frequency is independent of the data-serving intervals; in offline mode it also sets the deep-sleep interval. A day file holds a limited number of entries, so pick an interval that fits a full day — the Data Log page shows a capacity table. When a day file fills up it wraps: it clears and starts again from the top, overwriting that day's earlier entries (so logging never stops; you keep the most recent readings).
- **Offline mode.** Enabling offline mode logs with WiFi completely off. The web interface is unavailable while offline; to read the data, press RESET then hold MODE until the LED turns blue to re-enter Config mode.

> Available in the ESP8285 build (TeHyBug universal and Mini) when an RTC + EEPROM module is attached. The slim generic (1MB) build for old / first-generation boards omits the RTC/EEPROM driver entirely.

## Port B (green) supported sensors:
* BME680
* BME280/BMP280
* DHT21/DHT22/AM2032 (in dht simulation mode)
* AHT20
* MAX44009
* DS18B20
* other i2c and one wire sensors (requires code modification)
  
### Pinmapping Port B
  
<img src="images/tehybug_port_b_pinmapping.png" width="300">

## Port A (black) supported sensors:
* DHT21/DHT22/AM2032 (in dht simulation mode)
* DS18B20
* ADC soil moisture sensor
* other ADC and one wire sensors (requires code modification)

### Pinmapping Port A
  
<img src="images/tehybug_port_a_pinmapping.png" width="300">

## Upload new firmware via web interface (recommended)

To update the firmware from OTA WebInterface open http://tehybug.local/update in your browser, if this doesnt work, try to find out its IP from your router admin menu or use any local network ip scanner app for your mobile phone to get the device ip and then open http://<ip_address<ip address>>/update with your browser.

## Firmware binaries
The prebuilt binaries in [`firmware/`](firmware/) are rebuilt automatically on every merge to `main`:

| File | Board | Notes |
| --- | --- | --- |
| `firmware/tehybug.ino.esp8285.bin` | TeHyBug universal (v2) and Mini TeHyBug (ESP8285) | recommended |
| `firmware/tehybug.ino.esp8285_debug.bin` | TeHyBug universal / Mini (ESP8285) | serial debug output enabled |
| `firmware/tehybug.ino.generic.bin` | Old / first-generation TeHyBug boards (esp-01 based, generic ESP8266, 1MB flash) | slimmed to fit 1MB and stay OTA-updatable; no BME680, no RTC/EEPROM data log, no https data push (plain http works) |

## How to program/flash the board (advanced users only)
To flash firmware use the `firmware/tehybug.ino.esp8285.bin` file.
For flashing and programming you can use ARDUINO IDE, select there generic ESP8285 board.
Also you can use the [ESPTool](https://github.com/espressif/esptool) to flash binaries to the board or other tools (e.g. [NodeMCU PyFlasher](https://github.com/marcelstoer/nodemcu-pyflasher)) which are described at: https://nodemcu.readthedocs.io/en/latest/flash/

### Using `flash.sh` (esptool wrapper)
[`flash.sh`](flash.sh) wraps esptool with the same options as NodeMCU PyFlasher
(port, firmware, baud, flash mode, erase). It auto-detects the USB-serial port
and defaults to the ESP8285 build:

A pure-python esptool + pyserial are bundled in [`tools/vendor`](tools/vendor),
so no install is needed — just `python3`. (A system esptool is used instead if
one is found.)

```sh
./flash.sh                    # flash firmware/tehybug.ino.esp8285.bin to the auto-found port
./flash.sh -e                 # erase all flash first ("yes, wipes all data")
./flash.sh -p /dev/cu.usbserial-110 -b 460800
./flash.sh -l                 # list detected serial ports
./flash.sh -h                 # all options
```

### GUI: `flashUI.sh`
Prefer a window? [`flashUI.sh`](flashUI.sh) launches a small Tkinter GUI
([tools/flashui.py](tools/flashui.py)) that combines the flasher options (port, firmware,
baud, flash mode, erase) **and a built-in serial monitor** — one window instead
of separate PyFlasher + CoolTerm, so you never hit the "port busy" clash. After
a successful flash it auto-opens the monitor to show the boot log.

```sh
brew install python-tk@3.10   # tkinter, one-time (any python-tk works)
./flashUI.sh
```

esptool is bundled (`tools/vendor`), so flashing needs no install. The serial
monitor reads the port directly via `stty`, so pyserial isn't needed either.

### Using esptool directly
Replace /dev/cu.usbserial-1410 with your usb2serial port.

```esptool.py --port=/dev/cu.usbserial-1410  write_flash 0x00000 desired_tehybug_firmware.bin```



## Web Gui
  
<img src="images/webgui.png" width="800">

Demo web configuration page: https://tehybug.com/tehybug/v1/html/demo.html

## Configuration first steps
- Connect an external sensor to the board 3,5mm audio jack connector.
- Connect the power supply to micro USB port
- TeHyBug will boot, the LED will turn solid blue
- Connect to a TeHyBug wifi network like the image below (Password: TeHyBug123)
- <img src="images/wifimanager.png" width="350">
- open http://192.168.4.1/ in your browser, and click the configuration button
- <img src="images/credentials.png" width="350">
- Provide credentials of your WIFI network and save them
- If your credentials were correct, the TeHyBug WIFI network will disapear
- TeHyBug will connect to your network and boot in a configuration mode with solid blue LED light
- open with your browser http://tehybug.local/ and the configuration page should open. (if this didnt work. Find out the TeHyBug IP Addres from your router and open it with yoour browser)
- Follow the instructions on the configuration page.

## Factory reset
To delete all the configs, reset the wifi configuration and erase the on-device data log (the RTC + EEPROM module, if attached).

1. hit the RESET button
2. after that push and hold the MODE button for 20 seconds untill the LED turns red
3. release the MODE button.

The EEPROM data log is wiped only after the MODE button is released (it shares the I²C line with the button).

## Repository layout

- [`tehybug-universal.ino`](tehybug-universal.ino) — the sketch (entry point), at the repo root
- [`src/`](src/) — the firmware module headers; the sketch includes them as one translation unit
- [`firmware/`](firmware/) — prebuilt, flashable binaries
- [`libraries/`](libraries/) — vendored Arduino libraries (pinned, known-good versions)
- [`html/`](html/) — the PHP/JS/CSS configuration web UI (hosted at tehybug.com)
- [`tests/`](tests/) — native host tests + clang-tidy static analysis
- [`build.sh`](build.sh) / [`platformio.ini`](platformio.ini) — the two build paths
- [`flash.sh`](flash.sh) — esptool wrapper to flash a built binary over serial
- [`flashUI.sh`](flashUI.sh) / [`tools/flashui.py`](tools/flashui.py) — GUI flasher + serial monitor
- [`tools/vendor/`](tools/vendor) — bundled pure-python esptool + pyserial (so flashing needs no install)

## Building from source

Requirements: [arduino-cli](https://arduino.github.io/arduino-cli/) and git. Everything else is pinned:

- All Arduino libraries are vendored in [`libraries/`](libraries/) — exact known-good versions, including a PubSubClient patched to `MQTT_MAX_PACKET_SIZE 4000` (required for the Home Assistant discovery messages).
- [`ci/install-deps.sh`](ci/install-deps.sh) installs the ESP8266 core 2.7.4 and applies the `platform.local.txt` override needed to link the precompiled BSEC (BME680) library.

```bash
./ci/install-deps.sh        # one-time: install the ESP8266 toolchain
./build.sh                  # build for ESP8285 (default)
./build.sh all              # build esp8285 + generic
./build.sh esp8285 debug    # build with serial debug output
```

The flashable binary is placed in `firmware/` as `firmware/tehybug.ino.<variant>.bin`.

### PlatformIO

[`platformio.ini`](platformio.ini) mirrors the same board options, flags and
vendored libraries, so the project also builds with [PlatformIO](https://platformio.org/).
Either drive it directly:

```bash
pio run -e esp8285        # universal board (recommended)
pio run -e generic        # old/first-gen TeHyBug / 1 MB
pio run -e esp8285_debug  # with serial debug output
```

…or pick the backend from `build.sh` with the `TOOL` env var (same variant /
mode arguments, so one command line works for both tools):

```bash
./build.sh esp8285                    # arduino-cli (default)
TOOL=platformio ./build.sh esp8285    # same build via PlatformIO
TOOL=platformio ./build.sh all debug  # esp8285_debug + generic_debug
```

The sketch stays at the repo root (so the arduino-cli build is unchanged);
PlatformIO compiles it as a single translation unit and writes its output to
`.pio/build/<env>/firmware.bin`. The arduino-cli `build.sh` remains the
reference the CI release uses.

### Tests

The hardware-independent firmware logic (the EEPROM data log + date index, the
`common_functions` helpers, I²C device detection, and the boot/serve decision
logic) has native host tests that run on a desktop compiler — no board or
Arduino toolchain needed — plus a clang-tidy static-analysis pass. See
[`tests/`](tests/README.md):

```bash
./tests/run.sh    # native host tests (fake I²C EEPROM; 88 assertions)
./tests/tidy.sh   # clang-tidy over the host-compilable headers
```

## Development

Active development happens on the `development` branch.

Every push and pull request runs the [tests workflow](.github/workflows/tests.yml):
the native host tests, the clang-tidy static analysis, and a PlatformIO build of
the `esp8285` and `generic` environments.

Every pull request to `main` is also built by the [build workflow](.github/workflows/build.yml)
(arduino-cli) and the resulting binaries are attached as workflow artifacts. After a merge to
`main`, that workflow rebuilds all firmware variants, commits the updated binaries back to
the repository and publishes a [release](https://github.com/gumslone/tehybug-universal/releases)
with the binaries attached. The release tag (`vYYMMDDHHMM`) matches the firmware version
reported by the device.
