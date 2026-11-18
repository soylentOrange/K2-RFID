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
};
