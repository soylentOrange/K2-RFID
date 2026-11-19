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

  // A saved access point (BSSID) ties the thingy - and SafeBoot, which uses the same settings -
  // to that one access point: in a mesh it could never use another one. Current ESPConnect
  // versions and Improv don't save it anymore, older ones did: remove it.
  preferences.begin("espconnect", false);
  if (preferences.isKey("bssid") && preferences.getString("bssid").length()) {
    LOGW(TAG, "Removing the saved access point %s, so that any access point of %s can be used", preferences.getString("bssid").c_str(), ssid.c_str());
    preferences.remove("bssid");
  }
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
            const uint8_t* b = info.wifi_sta_disconnected.bssid;
            LOGW(TAG, "WiFi disconnected: reason %u (%s), RSSI %d dBm, access point %02X:%02X:%02X:%02X:%02X:%02X", info.wifi_sta_disconnected.reason,
                 WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(info.wifi_sta_disconnected.reason)), info.wifi_sta_disconnected.rssi,
                 b[0], b[1], b[2], b[3], b[4], b[5]);
          }
          _lastDisconnectReason = info.wifi_sta_disconnected.reason;
          break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          LOGI(TAG, "WiFi connected: access point %s, channel %d, RSSI %d dBm", WiFi.BSSIDstr().c_str(), WiFi.channel(), WiFi.RSSI());
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
      LOGI(TAG, "WiFi: access point %s, channel %d, RSSI %d dBm, TX power %.1f dBm, free internal RAM %u KB", WiFi.BSSIDstr().c_str(),
           WiFi.channel(), WiFi.RSSI(), WiFi.getTxPower() / 4.0f, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    }
    _roamCheck(now);
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

#define ROAM_WEAK_RSSI    -70              // look for a better access point below this signal
#define ROAM_MIN_GAIN     8                // switch only if the other one is this much stronger (dB)
#define ROAM_INTERVAL     (2 * 60 * 1000)  // at most one scan per interval (a scan pauses the WiFi briefly)

void ESPNetwork::_roamCheck(uint32_t now) {
  if (!_roamScan) {
    if (WiFi.RSSI() >= ROAM_WEAK_RSSI || (_lastRoamCheck != 0 && now - _lastRoamCheck < ROAM_INTERVAL))
      return;
    if (WiFi.scanComplete() != WIFI_SCAN_FAILED)
      return; // another scan (e.g. Improv) is running or its results are pending
    _lastRoamCheck = now;
    _roamScan = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
    return;
  }

  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING)
    return;
  _roamScan = false;
  if (n < 0)
    return;

  const String ssid = WiFi.SSID();
  const String current = WiFi.BSSIDstr();
  const int32_t rssi = WiFi.RSSI();
  int best = -1;
  for (int16_t i = 0; i < n; i++) {
    if (WiFi.SSID(i) != ssid || WiFi.BSSIDstr(i) == current)
      continue;
    if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))
      best = i;
  }
  if (best >= 0 && WiFi.RSSI(best) >= rssi + ROAM_MIN_GAIN) {
    LOGI(TAG, "WiFi: access point %s (channel %ld, %ld dBm) is stronger than %s (%ld dBm): switching...", WiFi.BSSIDstr(best).c_str(),
         static_cast<long>(WiFi.channel(best)), static_cast<long>(WiFi.RSSI(best)), current.c_str(), static_cast<long>(rssi));
    WiFi.scanDelete();
    // reconnecting scans all channels and picks the strongest access point
    WiFi.reconnect();
    return;
  }
  if (best >= 0)
    LOGI(TAG, "WiFi: weak signal (%ld dBm), best other access point %s has %ld dBm: staying", static_cast<long>(rssi),
         WiFi.BSSIDstr(best).c_str(), static_cast<long>(WiFi.RSSI(best)));
  else
    LOGI(TAG, "WiFi: weak signal (%ld dBm), no other access point of this network found", static_cast<long>(rssi));
  WiFi.scanDelete();
}

void ESPNetwork::_espConnectCallback() {
  _espConnect.loop();
}
