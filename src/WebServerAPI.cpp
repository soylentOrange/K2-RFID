// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <thingy.h>

#define TAG "WebServer"

// // gzipped assets
// extern const uint8_t logo_start[] asm("_binary__pio_embed_logo_captive_svg_gz_start");
// extern const uint8_t logo_end[] asm("_binary__pio_embed_logo_captive_svg_gz_end");

void WebServerAPI::begin(Scheduler* scheduler) {
  // Just to be sure that the static webserver is not running anymore before being started (again)
  _webServer->end();

  // Task handling
  _sr.setWaiting();
  _scheduler = scheduler;

  // create and run a task for setting up the webserver
  Task* webServerTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _webServerCallback(); }, _scheduler, false, NULL, NULL, true);
  webServerTask->enable();
}

void WebServerAPI::end() {
  LOGD(TAG, "Disabling WebServerAPI-Task...");
  LittleFS.end();
  _sr.setWaiting();
  _webServer->end();
  LOGD(TAG, "...done!");
}

// Start the webserver
void WebServerAPI::_webServerCallback() {
  LOGD(TAG, "Starting WebServerAPI...");

  // Start Filesystem
  if (!LittleFS.begin(false)) {
    LOGE(TAG, "An Error has occurred while mounting LittleFS!");
  } else {
    LOGD(TAG, "LittleFS mounted!");
    _fsMounted = true;
  }

  // Handle getting files from File System (will auto-magically serve the gzipped files)
  _webServer->serveStatic("/", LittleFS, "/")
    .setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });

  // serve logo for espConnect
  _webServer->serveStatic("/logo", LittleFS, "/logo_captive.svg")
    .setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });

  // clear persisted wifi config
  _webServer->on("/api/system/clearwifi", HTTP_POST, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Clearing WiFi configuration...");
    espNetwork.clearConfiguration();
    LOGW(TAG, TAG, "Restarting!");
    auto* response = request->beginResponse(200, "text/plain", "WiFi credentials are gone! Restarting now...");
    request->send(response);
    Mycila::System::restart(1000);
    led.setMode(LED::LEDMode::WAITING_CAPTIVE);
  });

  // do restart
  _webServer->on("/api/system/restart", HTTP_POST, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Restarting!");
    auto* response = request->beginResponse(200, "text/plain", "Restarting now...");
    request->send(response);
    Mycila::System::restart(1000);
    led.setMode(LED::LEDMode::WAITING_WIFI);
  });

  // reboot esp into SafeBoot
  _webServer->on("/api/system/safeboot", HTTP_POST, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Restarting in SafeBoot mode...");
    if (Mycila::System::restartFactory("safeboot", 1000)) {
      auto* response = request->beginResponse(200, "text/plain", "Restarting into SafeBoot now...");
      request->send(response);
      led.setMode(LED::LEDMode::NONE);
    } else {
      LOGW(TAG, "SafeBoot partition not found");
      auto* response = request->beginResponse(502, "text/plain", "SafeBoot partition not found!");
      request->send(response);
    }
  });

  // Set 404-handler only when the captive portal is not shown
  if (eventHandler.getNetworkState() != Mycila::ESPConnect::State::PORTAL_STARTED) {
    LOGD(TAG, "Register 404 handler in WebServerAPI");
    _webServer->onNotFound([](AsyncWebServerRequest* request) {
      LOGW(TAG, "Send 404 on request for %s", request->url().c_str());
      request->send(404);
    });
  } else {
    LOGD(TAG, "Skip registering 404 handler in WebServerAPI");
  }

  _webServer->begin();

  LOGD(TAG, "...done!");
  _sr.signalComplete();
}

StatusRequest* WebServerAPI::getStatusRequest() {
  return &_sr;
}
