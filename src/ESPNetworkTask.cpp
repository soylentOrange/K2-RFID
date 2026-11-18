// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <Preferences.h>
#include <thingy.h>

#include <string>

#define TAG "ESPNetwork"

void ESPNetwork::begin(Scheduler* scheduler) {
  LOGD(TAG, "Schedule ESPConnect...");
  // stop possibly running espConnect first
  if (_espConnect.getState() != Mycila::ESPConnect::State::NETWORK_DISABLED)
    _espConnect.end();

  // get some info from espconnect's preferences
  Preferences preferences;
  preferences.begin("espconnect", true);
  std::string ssid;
  if (preferences.isKey("ssid"))
    ssid = preferences.getString("ssid").c_str();
  bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  preferences.end();

  preferences.begin("k2rfid", true);
  _lowTxPower = preferences.getBool("lowtx", false);
  preferences.end();

  // the transmit power is reset whenever the WiFi (re)starts: apply it on every start
  if (!_txPowerListener) {
    _txPowerListener = true;
    WiFi.onEvent([this](arduino_event_id_t event, __unused arduino_event_info_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_START || event == ARDUINO_EVENT_WIFI_AP_START)
        _applyTxPower();
    });
  }

  if (ssid.empty() || ap) {
    LOGI(TAG, "Trying to start captive portal in the background...");
  } else {
    LOGI(TAG, "Trying to connect to saved WiFi (%s) in the background...", ssid.c_str());
  }

  // configure and begin espConnect
  _espConnect.setAutoRestart(true);
  _espConnect.setBlocking(false);
  _espConnect.setCaptivePortalTimeout(ESPCONNECT_TIMEOUT_CAPTIVE_PORTAL);
  _espConnect.setConnectTimeout(ESPCONNECT_TIMEOUT_CONNECT);
  _espConnect.begin(APP_NAME, CAPTIVE_PORTAL_SSID, CAPTIVE_PORTAL_PASSWORD);

  // Task handling
  _scheduler = scheduler;
  _espConnectTask = new Task(TASK_IMMEDIATE, TASK_FOREVER, [&] { _espConnectCallback(); }, _scheduler, false, NULL, NULL, true);
  _espConnectTask->enable();

  LOGD(TAG, "ESPConnect is scheduled for start...");
}

void ESPNetwork::end() {
  LOGD(TAG, "Stopping ESPConnect...");
  _espConnectTask->disable();
  _espConnect.end();
  LOGD(TAG, "...done!");
}

void ESPNetwork::setLowTxPower(bool low) {
  _lowTxPower = low;
  Preferences preferences;
  preferences.begin("k2rfid", false);
  preferences.putBool("lowtx", low);
  preferences.end();
  _applyTxPower();
}

void ESPNetwork::_applyTxPower() {
  if (WiFi.getMode() == WIFI_MODE_NULL)
    return;
  const wifi_power_t power = _lowTxPower ? WIFI_LOW_TX_POWER : WIFI_POWER_19_5dBm;
  WiFi.setTxPower(power);
  LOGI(TAG, "WiFi transmit power: %.1f dBm", WiFi.getTxPower() / 4.0f);
}

Mycila::ESPConnect* ESPNetwork::getESPConnect() {
  return &_espConnect;
}

void ESPNetwork::clearConfiguration() {
  _espConnect.clearConfiguration();
}

// Loop espConnect
void ESPNetwork::_espConnectCallback() {
  _espConnect.loop();
}
