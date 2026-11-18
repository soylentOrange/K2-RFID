// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <ImprovSerial.h>
#include <WiFi.h>
#include <thingy.h>

#include <algorithm>

#define TAG "Improv"

extern const char* __COMPILED_APP_VERSION__;

namespace {
  // https://www.improv-wifi.com/serial/
  constexpr uint8_t IMPROV_VERSION = 1;
  constexpr size_t HEADER_LEN = 9; // "IMPROV", version, type, length

  enum : uint8_t { TYPE_CURRENT_STATE = 0x01, TYPE_ERROR_STATE = 0x02, TYPE_RPC = 0x03, TYPE_RPC_RESULT = 0x04 };
  enum : uint8_t { STATE_READY = 0x02, STATE_PROVISIONING = 0x03, STATE_PROVISIONED = 0x04 };
  enum : uint8_t { ERROR_NONE = 0x00, ERROR_INVALID_RPC = 0x01, ERROR_UNKNOWN_RPC = 0x02, ERROR_UNABLE_TO_CONNECT = 0x03 };
  enum : uint8_t {
    RPC_WIFI_SETTINGS = 0x01,
    RPC_CURRENT_STATE = 0x02,
    RPC_DEVICE_INFO = 0x03,
    RPC_WIFI_NETWORKS = 0x04,
    RPC_NETWORK_STATE = 0x07,
  };
  enum : uint8_t { NETWORK_ONLINE = 0x01, NETWORK_SUPPORTS_WIFI = 0x02 };

  // give up on the credentials after this time (ESPConnect waits 20 s as well)
  constexpr uint32_t PROVISIONING_TIMEOUT = 20000;
  // WiFi.status() reports stale errors right after WiFi.begin(): only trust failures after this time
  constexpr uint32_t PROVISIONING_MIN_TIME = 5000;
  // time for the installer to receive the result before the thingy restarts with the new credentials
  constexpr uint32_t RESTART_DELAY = 3000;
  constexpr uint32_t SCAN_TIMEOUT = 15000;
  // drop an incomplete packet after this time
  constexpr uint32_t PACKET_TIMEOUT = 1000;

  const char HEADER[] = {'I', 'M', 'P', 'R', 'O', 'V'};

  bool isConnected() {
    return eventHandler.getNetworkState() == Mycila::ESPConnect::State::NETWORK_CONNECTED;
  }
} // namespace

void ImprovSerial::begin(Scheduler* scheduler) {
  // USB-CDC (S2/S3) or UART: harmless if it's already running
  Serial.begin(MONITOR_SPEED);
  _buffer.reserve(HEADER_LEN + 256);
  _task = new Task(10, TASK_FOREVER, [&] { _poll(); }, scheduler, false, NULL, NULL, true);
  _task->enable();
  LOGD(TAG, "Improv Serial ready");
}

void ImprovSerial::_poll() {
  if (_restartAt && static_cast<int32_t>(millis() - _restartAt) >= 0) {
    LOGI(TAG, "Restarting with the new WiFi credentials...");
    Serial.flush();
    ESP.restart();
  }

  if (_provisioning == Provisioning::TESTING)
    _processProvisioning();

  if (_scanRequested)
    _processScan();

  if (!_buffer.empty() && millis() - _lastByte > PACKET_TIMEOUT)
    _buffer.clear();

  while (Serial.available() > 0) {
    const uint8_t byte = Serial.read();
    _lastByte = millis();

    // look for the header, everything else (e.g. new lines) is ignored
    if (_buffer.size() < sizeof(HEADER)) {
      if (byte == HEADER[_buffer.size()]) {
        _buffer.push_back(byte);
      } else {
        _buffer.clear();
        if (byte == HEADER[0])
          _buffer.push_back(byte);
      }
      continue;
    }

    _buffer.push_back(byte);
    if (_buffer.size() < HEADER_LEN)
      continue;

    const size_t packetLen = HEADER_LEN + _buffer[8] + 1; // + checksum
    if (_buffer.size() < packetLen)
      continue;

    uint8_t checksum = 0;
    for (size_t i = 0; i < packetLen - 1; i++)
      checksum += _buffer[i];

    if (_buffer[6] != IMPROV_VERSION) {
      LOGW(TAG, "Unsupported version %d", _buffer[6]);
    } else if (checksum != _buffer[packetLen - 1]) {
      LOGW(TAG, "Invalid checksum");
      _sendError(ERROR_INVALID_RPC);
    } else {
      _handlePacket(_buffer[7], _buffer.data() + HEADER_LEN, _buffer[8]);
    }
    _buffer.clear();
  }
}

void ImprovSerial::_handlePacket(uint8_t type, const uint8_t* data, uint8_t len) {
  if (type != TYPE_RPC)
    return;
  // RPC: command, length of the data, data
  if (len < 2 || data[1] + 2 > len) {
    _sendError(ERROR_INVALID_RPC);
    return;
  }
  _handleRpc(data[0], data + 2, data[1]);
}

void ImprovSerial::_handleRpc(uint8_t command, const uint8_t* data, uint8_t len) {
  switch (command) {
    case RPC_CURRENT_STATE:
      _sendState();
      if (_provisioning == Provisioning::DONE || (_provisioning == Provisioning::IDLE && isConnected()))
        _sendRpcResult(RPC_CURRENT_STATE, {_url()});
      break;

    case RPC_DEVICE_INFO: {
      std::string name = espNetwork.getESPConnect()->getConfig().hostname.c_str();
      if (name.empty())
        name = APP_NAME;
      _sendRpcResult(RPC_DEVICE_INFO, {APP_NAME, __COMPILED_APP_VERSION__, ESP.getChipModel(), name});
      break;
    }

    case RPC_NETWORK_STATE: {
      const bool online = isConnected();
      std::vector<std::string> result{std::to_string(NETWORK_SUPPORTS_WIFI | (online ? NETWORK_ONLINE : 0))};
      if (online)
        result.push_back(_url());
      _sendRpcResult(RPC_NETWORK_STATE, result);
      break;
    }

    case RPC_WIFI_SETTINGS: {
      // ssid length, ssid, password length, password
      if (len < 1 || data[0] + 2 > len || data[0] + 2 + data[data[0] + 1] > len || data[0] == 0 || data[0] > 32 ||
          data[data[0] + 1] > 64) {
        _sendError(ERROR_INVALID_RPC);
        return;
      }
      const std::string ssid(reinterpret_cast<const char*>(data + 1), data[0]);
      const std::string password(reinterpret_cast<const char*>(data + data[0] + 2), data[data[0] + 1]);
      _startProvisioning(ssid, password);
      break;
    }

    case RPC_WIFI_NETWORKS:
      if (_provisioning != Provisioning::IDLE) {
        _sendRpcResult(RPC_WIFI_NETWORKS, {});
      } else if (!_scanRequested) {
        _scanRequested = true;
        _scanStart = millis();
      }
      break;

    default:
      _sendError(ERROR_UNKNOWN_RPC);
      break;
  }
}

void ImprovSerial::_startProvisioning(const std::string& ssid, const std::string& password) {
  if (_provisioning != Provisioning::IDLE) {
    _sendError(ERROR_INVALID_RPC);
    return;
  }
  LOGI(TAG, "Received WiFi credentials for %s", ssid.c_str());
  _sendError(ERROR_NONE);
  _ssid = ssid;
  _password = password;
  _provisioning = Provisioning::TESTING;
  _provisioningStart = millis();
  _sendState();

  if (isConnected()) {
    // the thingy is in a WiFi already: testing the new credentials would drop that
    // connection, so they are saved and used after the restart (the captive portal
    // opens again if they don't work)
    _processProvisioning();
    return;
  }

  // test the credentials while the captive portal (or the AP) keeps running
  _scanRequested = false;
  WiFi.scanDelete();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.begin(_ssid.c_str(), _password.c_str());
}

void ImprovSerial::_processProvisioning() {
  const bool alreadyConnected = isConnected();
  const wl_status_t status = WiFi.status();
  const uint32_t elapsed = millis() - _provisioningStart;

  if (alreadyConnected || status == WL_CONNECTED) {
    LOGI(TAG, "WiFi credentials %s, saving them", alreadyConnected ? "received" : "work");
    Mycila::ESPConnect::Config config = espNetwork.getESPConnect()->getConfig();
    if (config.hostname.empty())
      config.hostname = APP_NAME;
    config.wifiSSID = _ssid.c_str();
    config.wifiPassword = _password.c_str();
    config.wifiBSSID = "";
    config.apMode = false;
    Mycila::ESPConnect::saveConfiguration(config);

    _provisioning = Provisioning::DONE;
    _sendState();
    _sendRpcResult(RPC_WIFI_SETTINGS, {_url()});
    _restartAt = millis() + RESTART_DELAY;
    return;
  }

  const bool failed = status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST;
  if ((failed && elapsed >= PROVISIONING_MIN_TIME) || elapsed >= PROVISIONING_TIMEOUT) {
    LOGW(TAG, "Connecting to %s failed (status %d)", _ssid.c_str(), status);
    WiFi.disconnect(false);
    _provisioning = Provisioning::IDLE;
    _ssid.clear();
    _password.clear();
    _sendError(ERROR_UNABLE_TO_CONNECT);
    _sendState();
  }
}

void ImprovSerial::_processScan() {
  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING && millis() - _scanStart < SCAN_TIMEOUT)
    return;
  if (n == WIFI_SCAN_FAILED && millis() - _scanStart < SCAN_TIMEOUT) {
    WiFi.scanNetworks(true);
    return;
  }

  std::vector<std::string> seen;
  for (int16_t i = 0; i < n; i++) {
    const std::string ssid = WiFi.SSID(i).c_str();
    if (ssid.empty() || std::find(seen.begin(), seen.end(), ssid) != seen.end())
      continue;
    seen.push_back(ssid);
    _sendRpcResult(RPC_WIFI_NETWORKS, {ssid, std::to_string(WiFi.RSSI(i)), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
  }
  // an empty result ends the list
  _sendRpcResult(RPC_WIFI_NETWORKS, {});
  WiFi.scanDelete();
  _scanRequested = false;
}

std::string ImprovSerial::_url() const {
  if (_provisioning == Provisioning::DONE && !isConnected()) {
    // the address in the new WiFi (the thingy restarts with it)
    return std::string("http://") + WiFi.localIP().toString().c_str() + "/";
  }
  if (isConnected())
    return std::string("http://") + espNetwork.getESPConnect()->getIPAddress().toString().c_str() + "/";
  return "http://k2rfid.local/";
}

void ImprovSerial::_sendState() {
  uint8_t state = STATE_READY;
  if (_provisioning == Provisioning::TESTING)
    state = STATE_PROVISIONING;
  else if (_provisioning == Provisioning::DONE || isConnected())
    state = STATE_PROVISIONED;
  _sendPacket(TYPE_CURRENT_STATE, &state, 1);
}

void ImprovSerial::_sendError(uint8_t error) {
  _sendPacket(TYPE_ERROR_STATE, &error, 1);
}

void ImprovSerial::_sendRpcResult(uint8_t command, const std::vector<std::string>& strings) {
  std::vector<uint8_t> data{command, 0};
  for (const auto& s : strings) {
    const size_t len = std::min<size_t>(s.size(), 255);
    data.push_back(len);
    data.insert(data.end(), s.begin(), s.begin() + len);
  }
  data[1] = data.size() - 2;
  _sendPacket(TYPE_RPC_RESULT, data.data(), data.size());
}

void ImprovSerial::_sendPacket(uint8_t type, const uint8_t* data, size_t len) {
  std::vector<uint8_t> packet(HEADER, HEADER + sizeof(HEADER));
  packet.push_back(IMPROV_VERSION);
  packet.push_back(type);
  packet.push_back(len);
  packet.insert(packet.end(), data, data + len);
  uint8_t checksum = 0;
  for (uint8_t b : packet)
    checksum += b;
  packet.push_back(checksum);
  packet.push_back('\n');
  Serial.write(packet.data(), packet.size());
}
