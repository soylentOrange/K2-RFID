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

  private:
    Task* _espConnectTask = nullptr;
    void _espConnectCallback();
    Scheduler* _scheduler = nullptr;
    AsyncWebServer* _webServer;
    Mycila::ESPConnect _espConnect;
};
