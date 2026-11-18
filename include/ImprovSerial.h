// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <Arduino.h>
#include <TaskSchedulerDeclarations.h>

#include <string>
#include <vector>

// Improv Wi-Fi via the USB serial port (https://www.improv-wifi.com/serial/).
// Lets the firmware installer on GitHub Pages (ESP Web Tools) send the WiFi credentials
// right after flashing, instead of going through the captive portal.
class ImprovSerial {
  public:
    void begin(Scheduler* scheduler);

  private:
    enum class Provisioning { IDLE, TESTING, DONE };

    void _poll();
    void _handlePacket(uint8_t type, const uint8_t* data, uint8_t len);
    void _handleRpc(uint8_t command, const uint8_t* data, uint8_t len);
    void _startProvisioning(const std::string& ssid, const std::string& password);
    void _processProvisioning();
    void _processScan();

    void _sendState();
    void _sendError(uint8_t error);
    void _sendRpcResult(uint8_t command, const std::vector<std::string>& strings);
    void _sendPacket(uint8_t type, const uint8_t* data, size_t len);
    std::string _url() const;

    Task* _task = nullptr;
    std::vector<uint8_t> _buffer;
    uint32_t _lastByte = 0;

    Provisioning _provisioning = Provisioning::IDLE;
    std::string _ssid;
    std::string _password;
    uint32_t _provisioningStart = 0;
    uint32_t _restartAt = 0;

    bool _scanRequested = false;
    uint32_t _scanStart = 0;
};
