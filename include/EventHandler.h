// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <ESPNetworkTask.h>
#include <MycilaESPConnect.h>
#include <TaskSchedulerDeclarations.h>
#include <SpoolData.h>

#include <string>

class EventHandler {
  public:
    explicit EventHandler(ESPNetwork& espNetwork) : _espNetwork(&espNetwork) {}
    void begin(Scheduler* scheduler);
    void end();
    Mycila::ESPConnect::State getNetworkState();

  private:
    void _networkStateCallback(Mycila::ESPConnect::State state);
    Mycila::ESPConnect::State _networkState = Mycila::ESPConnect::State::NETWORK_DISABLED;
    Scheduler* _scheduler = nullptr;
    ESPNetwork* _espNetwork = nullptr;
};
