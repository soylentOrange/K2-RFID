# Changelog

## Unreleased

### Fixes

- Fixed a boot loop of v1.2.0 (no WiFi/captive portal, no USB): `WebServerAPI::begin()` called `AsyncWebServer::end()` in `setup()` before the network stack was up. Since AsyncTCP 3.5 this takes the lwIP core lock and asserts.
- Building the factory image now fails if the SafeBoot or file system build fails (e.g. `pio` not on the `PATH`), instead of silently packing a stale SafeBoot image and no file system.

## v1.2.0

### Highlights

- **New web app, also on GitHub Pages.** The web interface was rebuilt as a maintainable Vite project (`web/`). It is still served by the thingy at http://k2rfid.local, and is now also available at https://soylentorange.github.io/K2-RFID/ (Chromium-based browsers only, via Local Network Access; Safari/iOS and Firefox keep using the on-device app).
- **Material database via SSH.** `Update Database` now logs into the printer via SSH and copies `/mnt/UDISK/creality/userdata/box/material_database.json` (gzipped, ~25 KB). Current K2/K2 Plus firmware no longer serves the database over http; the old http path is still tried as a fallback. Printer address and root password (default `creality_2024`) are set in the new **Printer** settings section (#8).
- **Upload Database.** A `material_database.json` can be uploaded manually from the browser (#5).
- **More printers.** Printer discovery detects K2, K2 Pro, K2 Plus and Hi.
- **Verified tag writes.** A write is only reported as successful (beep/LED) after the tag has been re-read with its key and the data matches. Writes rejected by the tag are now detected and retried (#9).

### Other changes

- Materials from printer databases are de-duplicated (user presets share the id of their base material); brands come from the database.
- The last selected material is remembered.
- Web app: the settings are split into `Settings` and `Database` pages, so every page fits on one screen (compact layout for small screens). A new `Web Log` button opens the thingy's `/weblog` (#13).
- Web app: the color dialog offers the official colors of the selected material as presets (from the material database, Creality's colors are built in) and shows the name of a preset color.
- Updated libraries: ArduinoJson 7.4.3, AsyncTCP 3.6.1, ESPAsyncWebServer 3.12.1, MycilaESPConnect 10.6.3, MycilaSystem 4.1.2, FastLED 3.10.9, Adafruit_BusIO 1.17.4; new: LibSSH-ESP32 5.10.0.
- SafeBoot: Tasmota platform 2025.11.30, MycilaESPConnect 10.6.3 (matches MycilaSafeBoot v3.3.9) (#11).
- CI builds the firmware for all boards; release images are built and attached automatically.
- Building no longer needs globally installed `html-minifier-terser`/`clean-css`; npm dependencies are pinned in `web/package-lock.json` (also used for the SafeBoot website).
- Fixed a C++20 ambiguity warning in `SpoolData::operator==`.

### Installation

Download the factory image for your board and flash it with esptool v5:

    esptool write-flash 0x0 lolin_s2_mini.firmware.factory.bin

(older esptool: `esptool.py write_flash 0x0 …`). For updates of an already flashed thingy, the `*.firmware.bin` can be uploaded via SafeBoot/OTA.

| Board | Factory image | App (OTA) |
|---|---|---|
| Lolin S2 mini | `lolin_s2_mini.firmware.factory.bin` | `lolin_s2_mini.firmware.bin` |
| Lolin S2 mini + external WS2812b | `lolin_s2_mini_external_WS2812b.firmware.factory.bin` | `lolin_s2_mini_external_WS2812b.firmware.bin` |
| Waveshare ESP32-S3 Tiny | `esp32_s3_tiny.firmware.factory.bin` | `esp32_s3_tiny.firmware.bin` |

**Not yet tested on real hardware:** the SSH database download and the GitHub Pages app.

## v1.1.1

- Add material database fetching for Creality Hi printer.

## v1.1.0

- Error checking while writing tags: read back data after writing, retry on error, reflect PN532 errors via LED (rapidly flashing red).
- Added Adafruit-PN532 as local library.
- Changed the start-up sequence of components.

## v1.0.1

- Reduced SPI frequency to 0.5 MHz and increased delays to stabilize SPI communication; repeat initialization on errors.

## v1.0.0

- Initial release.
