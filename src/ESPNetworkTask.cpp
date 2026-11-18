// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <Preferences.h>
#include <esp_heap_caps.h>
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
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
      switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
        case ARDUINO_EVENT_WIFI_AP_START:
          _applyTxPower();
          break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
          if (_disconnectedSince == 0) {
            _disconnectedSince = millis() | 1;
            LOGW(TAG, "WiFi disconnected: reason %u (%s), RSSI %d dBm", info.wifi_sta_disconnected.reason,
                 WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(info.wifi_sta_disconnected.reason)), info.wifi_sta_disconnected.rssi);
          }
          _lastDisconnectReason = info.wifi_sta_disconnected.reason;
          break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          if (_disconnectedSince != 0)
            LOGI(TAG, "WiFi reconnected after %lu s", static_cast<unsigned long>((millis() - _disconnectedSince) / 1000));
          _disconnectedSince = 0;
          break;
        default:
          break;
      }
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
  if (_wifiWatchTask == nullptr) {
    _wifiWatchTask = new Task(10 * TASK_SECOND, TASK_FOREVER, [&] { _wifiWatchCallback(); }, _scheduler, false, NULL, NULL, true);
    _wifiWatchTask->enable();
  }

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
void ESPNetwork::_wifiWatchCallback() {
  if (!(WiFi.getMode() & WIFI_STA) || (WiFi.getMode() & WIFI_AP)) {
    _disconnectedSince = 0; // captive portal or WiFi off: nothing to watch
    return;
  }
  const uint32_t now = millis();
  if (WiFi.isConnected()) {
    // the signal every 5 minutes, to see in the log how it was before a disconnect
    if (_lastStatusLog == 0 || now - _lastStatusLog >= 5 * 60 * 1000) {
      _lastStatusLog = now;
      LOGI(TAG, "WiFi: RSSI %d dBm, channel %d, TX power %.1f dBm, free internal RAM %u KB", WiFi.RSSI(), WiFi.channel(),
           WiFi.getTxPower() / 4.0f, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    }
    return;
  }
  // disconnected for a minute (the WiFi driver didn't manage to reconnect by itself): push it
  const uint32_t since = _disconnectedSince;
  if (since != 0 && now - since >= 60 * 1000 && (now - since) % (60 * 1000) < 10 * 1000) {
    LOGW(TAG, "WiFi disconnected for %lu s (last reason %u): reconnecting...", static_cast<unsigned long>((now - since) / 1000),
         _lastDisconnectReason);
    WiFi.reconnect();
  }
}

void ESPNetwork::_espConnectCallback() {
  _espConnect.loop();
}
