NOTE (soylentOrange): 
This is a modified version of MycilaWebSerial v8.1.1
* removed the input/command line
* changed the path for providing a logo
* removed src/MycilaWebSerialPage.h
* move portal/index.html to ../embed/webserial.html
* added a download button for the recorded text

# MycilaWebSerial

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaWebSerial.svg)](https://GitHub.com/mathieucarbou/MycilaWebSerial/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaWebSerial.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaWebSerial)

[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaWebSerial/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaWebSerial/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaWebSerial)](https://GitHub.com/mathieucarbou/MycilaWebSerial/commit/)
[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/mathieucarbou/MycilaWebSerial)

MycilaWebSerial is a Serial Monitor for ESP32 that can be accessed remotely via a web browser. Webpage is stored in program memory of the microcontroller.

This library is based on the UI from [asjdf/WebSerialLite](https://github.com/asjdf/WebSerialLite) (and this part falls under GPL v3).

## Changes

- Simplified callbacks
- Fixed UI
- Fixed Web Socket auto reconnect
- Fixed Web Socket client cleanup (See `WEBSERIAL_MAX_WS_CLIENTS`)
- Command history (up/down arrow keys) saved in local storage
- Support logo and fallback to title if not found.
- Arduino 3 / ESP-IDF 5.1 Compatibility
- Improved performance: can stream up to 20 lines per second is possible

To add a logo, add a handler for `/logo` to serve your logo in the image format you want, gzipped or not.
You can use the [ESP32 embedding mechanism](https://docs.platformio.org/en/latest/platforms/espressif32.html).

## Preview

![Preview](https://s2.loli.net/2022/08/27/U9mnFjI7frNGltO.png)

[DemoVideo](https://www.bilibili.com/video/BV1Jt4y1E7kj)

## Features

- Works on WebSockets
- Realtime logging
- Any number of Serial Monitors can be opened on the browser
- Uses Async Webserver for better performance
- Light weight (<3k)
- Timestamp
- Event driven

## Dependencies

- [ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)

## Usage

```c++
  WebSerial webSerial;

  webSerial.onMessage([](const std::string& msg) { Serial.println(msg.c_str()); });
  webSerial.begin(server);

  webSerial.print("foo bar baz");
```

If you need line buffering to use print(c), printf, write(c), etc:

```c++
  WebSerial webSerial;

  webSerial.onMessage([](const std::string& msg) { Serial.println(msg.c_str()); });
  webSerial.begin(server);

  webSerial.setBuffer(100); // initial buffer size

  webSerial.printf("Line 1: %" PRIu32 "\nLine 2: %" PRIu32, count, ESP.getFreeHeap());
  webSerial.println();
  webSerial.print("Line ");
  webSerial.print(3);
  webSerial.println();
```

`WebSerial` is a class. It is not a static instance anymore.
This allows you to initialize it only when needed in order to save memory space:

```c++
  WebSerial* webSerial = nullptr;

  void setup() {
    if (shouldEnableWebSerial) {
      webSerial = new WebSerial();
      webSerial->onMessage([](const std::string& msg) { Serial.println(msg.c_str()); });
      webSerial->begin(server);
    }
  }

  void loop() {
    if (webSerial) {
      webSerial->print("foo bar baz");
    }
  }
```
