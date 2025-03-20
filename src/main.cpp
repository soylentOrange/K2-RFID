// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <MycilaESPConnect.h>
#include <MycilaSystem.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);

String getEspID() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}

void setup() {
  // Start Serial or USB-CDC
#if !ARDUINO_USB_CDC_ON_BOOT
  Serial.begin(MONITOR_SPEED);
  // Only wait for serial interface to be set up when not using USB-CDC
  while (!Serial)
    continue;
#else
  // USB-CDC doesn't need a baud rate
  Serial.begin();

// Note: Enabling Debug via USB-CDC is handled via framework
#endif

  // this is possible to serve a logo
  // server.on("/logo", HTTP_GET, [&](AsyncWebServerRequest* request) {
  //   AsyncWebServerResponse* response = request->beginResponse(200, "image/png");
  //   response->addHeader("Content-Encoding", "gzip");
  //   response->addHeader("Cache-Control", "public, max-age=900");
  //   request->send(response);
  // });

  // clear persisted config
  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Clearing configuration...");
    espConnect.clearConfiguration();
    request->send(200);
    ESP.restart();
  });

  server.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Restarting...");
    request->send(200);
    ESP.restart();
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", "<form method='POST' action='/safeboot' enctype='multipart/form-data'><input type='submit' value='Restart in SafeBoot mode'></form>");
  });

  server.on("/safeboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Restarting in SafeBoot mode... Look for an Access Point named: SafeBoot-" + getEspID());
    Mycila::System::restartFactory("safeboot");
  });

  // network state listener is required here in async mode
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();

    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        // serve your home page here
        server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
                return request->send(200, "text/plain", "Hello World!");
              })
          .setFilter([](__unused AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
        server.begin();
        break;

      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        server.end();
        break;

      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  espConnect.begin(APP_NAME, CAPTIVE_PORTAL_SSID);

  Serial.println("====> setup() completed...");
}

void loop() {
  espConnect.loop();
}