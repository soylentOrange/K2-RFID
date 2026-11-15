# Changelog

## Unreleased

### New

- Printer discovery (`Search` on the `Database` page) also finds the K2-series models with the codes `F025`, `GS-04` and `Z2`. They share firmware and mainboard with the K2/K2 Pro and have a CFS; their material databases use the same material ids.

## v1.2.2

### Fixes

- `Update Database`: when the SSH download fails and the printer's factory default database is fetched via http instead, the web app now says so and shows why SSH failed (e.g. wrong root password). Before, it silently reported success, although the default database lacks materials synced from Creality Print and custom ones (#8).
- After `Update Database`, the web app showed the old database for up to 10 minutes, because the browser reused its cached copy. The database is now always loaded fresh from the thingy (#8).
- The database info in the settings now also shows the number of user presets (e.g. "101 materials, 19 presets"). Presets share the id of the material they are based on and are listed only once, so before, a database fetched from the printer looked exactly like the bundled one (#8).

### Installation

Only the web app changed, so updating via SafeBoot/OTA with `<board>.firmware.bin` is enough (WiFi credentials and the material database are kept). For a fresh board use `<board>.firmware.factory.bin` with esptool (`esptool write-flash 0x0 lolin_s2_mini.firmware.factory.bin`). Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.2.1

**Please use this release instead of v1.2.0, which boot-loops on real hardware.**

### Fixes

- Fixed a boot loop of v1.2.0 (no WiFi/captive portal, no USB): `WebServerAPI::begin()` called `AsyncWebServer::end()` in `setup()` before the network stack was up. Since AsyncTCP 3.5 this takes the lwIP core lock and asserts (#15).
- Building the factory image now fails if the SafeBoot or file system build fails (e.g. `pio` not on the `PATH`), instead of silently packing a stale SafeBoot image and no file system (#15).

### New

- Web app: the color dialog offers the official colors of the selected material as presets, like "Official Filament" in Creality Print (taken from the material database; the colors of Creality's filaments are built in). The name of a preset color is shown next to its hex value (#14).
- Updated the bundled material database to the current one of the K2 Plus (101 instead of 40 materials; adds eSUN and Polymaker, and the official colors of the Creality filaments). It is part of the file system image, so already flashed thingies get it via `Update Database`/`Upload Database` or by flashing a factory image (#17).

### Installation

Same files and flashing as for v1.2.0: `<board>.firmware.factory.bin` for esptool (`esptool write-flash 0x0 lolin_s2_mini.firmware.factory.bin`), `<board>.firmware.bin` for updates via SafeBoot/OTA. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.2.0

**Boot-loops on real hardware, use v1.2.1 instead.**

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
