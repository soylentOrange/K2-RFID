# Changelog

## v1.5.4-beta

### New

- WiFi diagnostics in the web log: the signal (RSSI, channel, TX power) and free internal RAM every 5 minutes, every disconnect with its reason, how long it took to reconnect, and the network state changes (e.g. connect timeout -> captive portal). The web log also goes to the USB serial port, from the start of the firmware on, to watch it while the WiFi is gone.

### Changes

- Web app, `Settings` page: the beeper switch moved into `Programmer Settings`, the `Annoyance` section is gone. This frees space, so `Low WiFi TX power` also fits in a browser window with address bar.

### Fixes

- WiFi watchdog: if the station stays disconnected for a minute, the thingy pushes a reconnect (every minute until it's back).

### Installation

Update via the `Firmware` page or SafeBoot/OTA with `<board>.firmware.bin`. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.5.3-beta

### Fixes

- `Update Database`: the small allocations of the SSH download (libssh, mbedtls key exchange) go to the PSRAM too. Before, they used the internal RAM (the lowest free internal RAM was down to 6 KB), which WiFi needs. The web log shows the memory before and after the download, including the lowest free internal RAM since boot.
- Every page of the web app fits on one screen again, in every browser. The layout no longer depends on fixed window heights (which didn't match the real height of the content in every browser and font, e.g. the `Settings` page of the GitHub Pages app with its `Device` section, or the firmware installer): the page measures itself and hides less important texts / uses the compact layout only as far as needed. The color picker on the main page takes the free height.
- `Settings` and `Database` pages: free height is spread between the sections instead of collecting at the bottom. On low screens the GitHub Pages app hides the `Device` heading instead of `Annoyance`, so the beeper switch no longer sticks to the address field.

### Installation

Update via the `Firmware` page or SafeBoot/OTA with `<board>.firmware.bin`. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.5.2-beta

### New

- `Low WiFi TX power` in the settings lowers the WiFi transmit power from 19.5 dBm to 8.5 dBm. For boards that drop out of the WiFi after a few minutes while the RFID reader keeps working: the LOLIN S2 Mini does that on weak power supplies (e.g. a computer's USB port). Applied right away and kept across restarts.

### Changed

- The SSH download of `Update Database` (its 24 KB stack and the download buffer) runs in the PSRAM now. The `_static-stack` variant reserved these 24 KB in the internal RAM, which left WiFi too little memory (9 to 18 KB free): the thingy dropped off the network after a few minutes while the RFID reader kept working. There's only one firmware per board again, the `_static-stack` builds and the switch on the `Firmware` page and in the installer are gone. `Thingy Info` shows "SSH download runs in PSRAM" (hover the RAM entry).

### Fixes

- Firmware installer: `Connect to Wi-Fi` (Improv) is offered for the LOLIN S2 Mini too, and the useless `Logs & Console` entry is hidden (the thingy logs via the web log).

### Installation

Updating via the `Firmware` page or SafeBoot/OTA with `<board>.firmware.bin` is enough (WiFi credentials and the material database are kept), also coming from a `_static-stack` build. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.5.1-beta

### Fixes

- `SafeBoot` in the settings waits until SafeBoot is up and then opens its page. From the GitHub Pages app it now opens the thingy's address (e.g. `http://k2rfid.local/`) instead of reloading the Pages app, so you don't have to type the address yourself.
- The `Firmware` page shows the newest pre-release next to the newest release (before, only the release was shown as "Newest", even with a newer pre-release installed).

### Changed

- The link to the firmware installer (ESP Web Tools) moved from the settings to the `Firmware` page, as a button in its own section `USB Install (ESP Web Tools)` (with the board of the thingy preselected). The WiFi update there is now titled `OTA Update (WiFi)`.

### Installation

Update from v1.5.0-beta on the `Firmware` page of the web app, or via SafeBoot/OTA with `<board>_static-stack.firmware.bin`. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.5.0-beta

**Pre-release for trying the firmware update over WiFi from GitHub.** Includes everything from v1.4.1-beta and v1.4.2-beta (firmware installer, WiFi setup via Improv).

### New

- Firmware update from GitHub on the new `Firmware` page of the web app (`Database >>`): pick the version and click `Update Firmware`. The browser downloads `<board>.firmware.bin` from the GitHub Pages site, restarts the thingy into SafeBoot and uploads the image there; afterwards it checks that the new version is running. WiFi credentials and the material database are kept. Board and variant (`_static-stack`) of the running firmware are preselected, the newest release is offered plus a newer pre-release. The browser does the https download, so neither the firmware nor SafeBoot need TLS and the flash layout stays the same.
- `/api/system/info` also reports the firmware version, the build (`buildName`) and whether it's a `_static-stack` build.
- The GitHub Pages site also hosts the app images (`<board>.firmware.bin`) of the newest release and pre-release.

### Installation

Firmware before this version has no `Firmware` page, so update once by hand via SafeBoot/OTA with `<board>_static-stack.firmware.bin` (WiFi credentials and the material database are kept). Later updates can be done from the web app. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.4.2-beta

**Pre-release for trying the WiFi setup from the firmware installer.**

### New

- WiFi setup over USB: the firmware speaks [Improv Serial](https://www.improv-wifi.com/serial/), so the [firmware installer](https://soylentorange.github.io/K2-RFID/install.html) can send the WiFi credentials (`Connect to Wi-Fi`, with a list of the WiFi networks nearby). The thingy tests them while the captive portal keeps running, saves them and restarts in your WiFi. Wrong credentials are reported and nothing is saved. If the thingy is already in a WiFi, the new credentials are saved and used after the restart (the captive portal opens again if they don't work).
- The installer tells more clearly that the LOLIN S2 Mini needs a press on its RST button after flashing (the S2 can't be reset over USB) and how to get to the WiFi setup afterwards: click `Install` again and pick the port of the running firmware.

### Installation

Updating via SafeBoot/OTA with `<board>_static-stack.firmware.bin` is enough. To try the WiFi setup, flash a board via https://soylentorange.github.io/K2-RFID/install.html?release=v1.4.2-beta. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.4.1-beta

**Pre-release for trying the firmware installer in the browser.**

### New

- Firmware installer on GitHub Pages: https://soylentorange.github.io/K2-RFID/install.html flashes the factory image with [ESP Web Tools](https://esphome.github.io/esp-web-tools/) right from Chrome or Edge, no esptool needed. Pick the board, the version (newest release or a newer pre-release) and the variant (`_static-stack` preselected), connect the board via USB and click `Install`. LOLIN S2 Mini: hold "0", press and release "RST", release "0" before installing, press "RST" afterwards.
- The settings page of the web app links to the installer.
- After publishing a release, the release workflow rebuilds the GitHub Pages site, which copies the factory images of the new release for the installer.

The installer is meant for the first installation: like every factory image it replaces the WiFi credentials and the material database. Updates still go over WiFi via SafeBoot with `<board>.firmware.bin`.

### Installation

The firmware is the same as in v1.4.0 apart from the link to the installer. Updating via SafeBoot/OTA with `<board>_static-stack.firmware.bin` is enough; to try the installer, flash a board via https://soylentorange.github.io/K2-RFID/install.html?release=v1.4.1-beta. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.4.0

Includes everything from [v1.3.1-beta](#v131-beta) (memory diagnostics, the `_static-stack` variant and the web log fixes).

### New

- User presets from a database copied from the printer (filament profiles that Creality Print syncs to the printer, e.g. `HyPLA blue@K2 Plus 0.4 nz - RW` or `JAYO PETG ...`) are listed in their own category `User Presets`, each name once. A tag only stores the material id, so a preset is written as the material it's based on; the heading above the selection shows which one (#8).
- The presets count on the `Database` page counts each preset once (Creality Print can sync the same preset several times).

### Fixes

- The material itself is always preferred over its presets, also when a preset comes first in the database or its name has no `@`.
- Long material names no longer widen the page.

### Installation

Updating via SafeBoot/OTA with `<board>.firmware.bin` is enough (WiFi credentials and the material database are kept). If `Update Database` reports "Not enough memory to start the download", use `<board>_static-stack.firmware.bin` instead, which reserves the memory for the download at boot (see [v1.3.1-beta](#v131-beta)). For a fresh board use the matching `.firmware.factory.bin` with esptool. Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.3.1-beta

**Pre-release for diagnosing memory problems with `Update Database` (#8).**

### New

- The settings page shows the chip (model, revision, flash size), the PSRAM and the free memory (free internal RAM, largest free block, lowest free internal RAM since boot). The same data is available as JSON at `http://k2rfid.local/api/system/info`.

### Fixes

- When the SSH download fails, the message now says why (timeout, SSH error, database too large, file system full) and includes the free memory and PSRAM. Before, it only said "Not enough memory to start the download" or "Error while downloading the database" (#8).

### Web log

- The four buttons (download, clear, pause auto-scrolling, time-stamps) sit in their own toolbar row next to the connection status instead of covering the log. The log fills the remaining height, active toggles are highlighted and the buttons stay visible on phones.
- The buttons of the embedded web log did nothing, because the minifier renamed the functions the inline `onclick` handlers pointed to. They are now bound in JavaScript.
- Time-stamped lines no longer get an extra blank line.
- The toasts look the same as in the main web app; Toastify is no longer needed and was removed from the file system image.

### Variant: reserved stack

This pre-release comes in two variants for every board:

- `<board>.firmware.bin`: as before, the 24 KB stack of the SSH download is allocated when `Update Database` is clicked. That fails with "Not enough memory to start the download" if no free block of that size is left.
- `<board>_static-stack.firmware.bin`: the stack is reserved right after the firmware starts, while the memory isn't fragmented yet (build flag `PRINTER_SSH_STATIC_STACK`), so starting the download can't fail that way. The 24 KB are taken for good, also when no download runs. `Thingy Info` shows "reserved for the SSH download" for this variant.

### Installation

Updating via SafeBoot/OTA with `<board>.firmware.bin` (or `<board>_static-stack.firmware.bin`) is enough (WiFi credentials and the material database are kept). For a fresh board use the matching `.firmware.factory.bin` with esptool (`esptool write-flash 0x0 lolin_s2_mini.firmware.factory.bin`). Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

## v1.3.0

### New

- Printer discovery (`Search` on the `Database` page) also finds the K2-series models with the codes `F025`, `GS-04` and `Z2`. They share firmware and mainboard with the K2/K2 Pro and have a CFS; their material databases use the same material ids.

### Fixes

- `Update Database` hung (spinner "Downloading the material database ... (via SSH)") when the printer was switched off: after the SSH timeout, the http fallback waited for the browser's own connection timeout, which can take minutes. The fallback now gives up after 10 s and the SSH error is shown (#8).

### Installation

Only the web app changed, so updating via SafeBoot/OTA with `<board>.firmware.bin` is enough (WiFi credentials and the material database are kept). For a fresh board use `<board>.firmware.factory.bin` with esptool (`esptool write-flash 0x0 lolin_s2_mini.firmware.factory.bin`). Boards: `lolin_s2_mini`, `lolin_s2_mini_external_WS2812b`, `esp32_s3_tiny`.

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
