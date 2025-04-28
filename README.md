# K2-RFID
 
Web app controlled and esp32-powered K2/CFS RFID programming thingy based on [DnG-Crafts/K2-RFID](https://github.com/DnG-Crafts/K2-RFID).

## Why do I need it?

Creality's K2 Plus printer supports 1k MIFARE tags to identify the spool's material and color.
As the tagged spools from Creality come with an additional price and also to retrofit older Creality or non-Creality spools, you could use this tool to simply write your own tags, add two of them per spool and enjoy the magic of automatically identified materials.

## What do I need to make one?

Apart from the source code of this project (and a computer running Platformio):
- esp32 microcontroller (tested with a [Waveshare ESP32-S3-Tiny board](https://www.waveshare.com/wiki/ESP32-S3-Tiny))
- PN532 NFC/RFID board
- MIFARE classic 1k tags
- housing (optionally)
- battery, charger, switch, piezo disk (optionally)

For minifying the html parts, you also need to install: 
- [html-minifier-terser](https://github.com/terser/html-minifier-terser)
  - `npm install html-minifier-terser -g`
- [clean-css](https://github.com/clean-css/clean-css)
  - `npm install clean-css-cli -g`

## How to use it?

Once you have programmed your esp32, connected it to the PN532 NFC/RFID board, and supplied it with power, a new access point (with SSID: K2RFID) will appear on your computer or phone. Set the WiFi credentials for your home network (or set it to AP-mode). After a restart of the device, just visit K2RFID.local. Here you can read and write the tags. Afterwards just put two of them (obviously with identical content) to a spool and load it into your printer's CFS.

### Select Material Settings via UI

Just select the color by hitting the large color circle to select the color of choice (or select one the the known - at least known to me - standard colors), the type of material and the amount of material on the spool. After hitting the `Apply` button, the programmer is armed and will wait for tags to write the content. A serial number is auto-generated and will be used for all subsequent writes. A new serial number is genereted whenever you hit the `Apply` button again.

<p align="center">
    <img src="assets/doc/screenshot_ui.jpeg" alt="screenshot UI" style="width:50%; height:auto;" >
</p>

### Copy/Clone Tag

Once a tag is read, just click the toast notification and it will make the content (color, material, weight, and serial number - in case `clone serial number` is active in the settings) of the tag available for writing. Just hit the `Apply` button to arm the programmer.

### Safeguarding existing Tags

By default, only empty tags are written. When you want to re-program tags, disable the `Write only empty tags` checkbox in the settings.

### Future Materials

As of now, a fixed set of materials (defined by the K2Plus's firmware) is known to the programmer. You can update the list by hitting the `Update Database` button in the setting. It will try to identify a K2Plus on your local network, download the material database and saves it onto the prgrammer. No warranty that future firmware updates might break this behaviour... 

<p align="center">
    <img src="assets/doc/screenshot_setting.jpeg" alt="screenshot settings" style="width:50%; height:auto;" >
</p>

## Device Feedback

I've added a most annyoing beeper to the programmer. You can switch it off in the settings.

### LED

On boards with an onboard LED the current state and events are shown via the LED.

#### RGB_BUILTIN

On boards with an onboard RGB-LED different colors are used: 

* Serving captive portal (connect to enter credentials):
  * rapidly blinking blue

* Waiting for network connection:
  * blinking blue

* Connected to WiFi (or in AP-mode) and waiting to read tags:
  * dim solid white

* Tag read:
  * flashing green

* Armed to write tags:
  * breathing orange

* Armed to write and re-write tags:
  * breathing red

* Tag written:
  * flashing red/orange

Note: just make sure that the RGB_BUILTIN_LED_COLOR_ORDER is set matching your board. Otherwise the colours will look strange.

#### LED_BUILTIN

On boards with an onboard mono-color LED: 

* Serving captive portal (connect to enter credentials):
  * blinking rapidly

* Waiting for network connection:
  * blinking

* Connected to WiFi (or in AP-mode) and waiting to read tags:
  * dim solid

* Tag read:
  * flashing

* Armed to write tags:
  * breathing

* Armed to write and re-write tags:
  * breathing quickly

* Tag written:
  * flashing

## Acknowledgements

* This project is based the project [DnG-Crafts/K2-RFID](https://github.com/DnG-Crafts/K2-RFID). I redesigned the website to my likings, and completely re-wrote the esp32 code. 

* The app icon is modified from [Solar Icons](https://www.figma.com/community/file/1166831539721848736?ref=svgrepo.com) in CC Attribution License via [SVG Repo](https://www.svgrepo.com/).

* The favicon was prepared according (loosely) the guide [How to Favicon in 2025](https://evilmartians.com/chronicles/how-to-favicon-in-2021-six-files-that-fit-most-needs). 

* Creating svgs with Inkscape leaves a lot of clutter in the file, [svgo](https://github.com/svg/svgo) helps.

* The Toast notifications are made with [Toastify](https://github.com/apvarun/toastify-js).

* Splashcreens for iOS are auto-generated with [iosPWASplash](https://github.com/avadhesh18/iosPWASplash).

* The formula for generating the breathing LED was found in a post at [ThingPulse](https://thingpulse.com/breathing-leds-cracking-the-algorithm-behind-our-breathing-pattern).
