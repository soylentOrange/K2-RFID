// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <MycilaESPConnect.h>
#include <TaskSchedulerDeclarations.h>

class ESPNetwork {
  public:
    explicit ESPNetwork(AsyncWebServer& webServer) : _webServer(&webServer), _espConnect(webServer) {}
    void begin(Scheduler* scheduler);
    void end();
    void clearConfiguration();
    Mycila::ESPConnect* getESPConnect();
    // reduced WiFi transmit power (for boards that lose the WiFi on a weak power supply)
    bool getLowTxPower() const { return _lowTxPower; }
    void setLowTxPower(bool low);

  private:
    Task* _espConnectTask = nullptr;
    void _espConnectCallback();
    Scheduler* _scheduler = nullptr;
    AsyncWebServer* _webServer;
    Mycila::ESPConnect _espConnect;
    bool _lowTxPower = false;
    bool _txPowerListener = false;
    void _applyTxPower();
    // WiFi diagnostics and watchdog: log disconnects (with reason) and the signal, and push a
    // reconnect when the station stays disconnected
    Task* _wifiWatchTask = nullptr;
    void _wifiWatchCallback();
    volatile uint32_t _disconnectedSince = 0; // millis(), 0: connected (or not in station mode)
    volatile uint8_t _lastDisconnectReason = 0;
    uint32_t _lastStatusLog = 0;
    // mesh networks: switch to a clearly stronger access point of the same network (the WiFi
    // driver only picks the strongest when it connects and doesn't roam by itself)
    uint32_t _lastRoamCheck = 0;
    bool _roamScan = false;
    void _roamCheck(uint32_t now);
};
