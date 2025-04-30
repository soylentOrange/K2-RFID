// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

#include <string>

class WebSite {
  public:
    explicit WebSite(AsyncWebServer& webServer) : _webServer(&webServer) { _sr.setWaiting(); }
    void begin(Scheduler* scheduler);
    void end();
    typedef std::function<void(JsonDocument doc)> SpooldataCallback;
    void listenSpooldata(SpooldataCallback callback) { _spooldataCallback = callback; }
    StatusRequest* getStatusRequest();

  private:
    void _webSiteCallback();
    void _wsCleanupCallback();
    Task* _wsCleanupTask = nullptr;
    Scheduler* _scheduler = nullptr;
    StatusRequest _sr;
    AsyncWebServer* _webServer;
    AsyncWebSocket* _ws = nullptr;
#ifdef USE_BEEPER
    bool _beepOnRW = false;
#endif
    bool _cloneSerial = false;
    SpooldataCallback _spooldataCallback = nullptr;
    void _tagReadCallback(CFSTag tag);
    void _tagWriteCallback(bool success);
    static void _denyUpload(AsyncWebServerRequest* request, __unused String filename, __unused size_t index, __unused uint8_t* data, __unused size_t len, __unused bool final) { // don't accept file uploads
      request->send(400);
    }
    static void _ignoreRequest(__unused AsyncWebServerRequest* request) {}
};
