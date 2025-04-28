// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <thingy.h>
#define TAG "EventHandler"

void EventHandler::begin(Scheduler* scheduler) {
  _networkState = _espNetwork->getESPConnect()->getState();

  // Task handling
  _scheduler = scheduler;

  // Register Callback to espConnect
  _espNetwork->getESPConnect()->listen([&](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    _networkStateCallback(state);
  });

  LOGD(TAG, "Registered EventHandler to ESPConnect...");
}

void EventHandler::end() {
  LOGD(TAG, "Disabling EventHandler...");
  _espNetwork->getESPConnect()->listen(nullptr);
  _networkState = Mycila::ESPConnect::State::NETWORK_DISABLED;
}

Mycila::ESPConnect::State EventHandler::getNetworkState() {
  return _networkState;
}

// Handle events from ESPConnect
void EventHandler::_networkStateCallback(Mycila::ESPConnect::State state) {
  _networkState = state;

  switch (state) {
    case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      LOGI(TAG, "--> Connected to network...");
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      webServerAPI.begin(_scheduler);
      webSite.begin(_scheduler);
      rfid.begin(_scheduler);
      led.setMode(LED::LEDMode::WAITING_READ);
      break;

    case Mycila::ESPConnect::State::AP_STARTED:
      LOGI(TAG, "--> Created AP...");
      LOGI(TAG, "SSID: %s", _espNetwork->getESPConnect()->getAccessPointSSID().c_str());
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      webServerAPI.begin(_scheduler);
      webSite.begin(_scheduler);
      rfid.begin(_scheduler);
      led.setMode(LED::LEDMode::WAITING_READ);
      break;

    case Mycila::ESPConnect::State::PORTAL_STARTED:
      LOGI(TAG, "--> Started Captive Portal...");
      LOGI(TAG, "SSID: %s", _espNetwork->getESPConnect()->getAccessPointSSID().c_str());
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      webServerAPI.begin(_scheduler);
      led.setMode(LED::LEDMode::WAITING_CAPTIVE);
      break;

    case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
      LOGI(TAG, "--> Disconnected from network...");
      led.setMode(LED::LEDMode::WAITING_WIFI);
      rfid.end();
      webSite.end();
      webServerAPI.end();
      break;

    case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
      LOGI(TAG, "--> Captive Portal has ended, auto-save the configuration...");
      auto config = _espNetwork->getESPConnect()->getConfig();
      LOGD(TAG, "ap: %d", config.apMode);
      LOGD(TAG, "wifiSSID: %s", config.wifiSSID.c_str());
      LOGD(TAG, "wifiPassword: %s", config.wifiPassword.c_str());
      led.setMode(LED::LEDMode::WAITING_WIFI);
      break;
    }

    default:
      break;
  } /* switch (state) */
}
