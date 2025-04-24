// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <CFSTag.h>
#include <ESPAsyncWebServer.h>
#include <ESPNetworkTask.h>
#include <EventHandler.h>
#include <FS.h>
#include <LittleFS.h>
#include <MycilaESPConnect.h>
#include <MycilaSystem.h>
#include <RFID.h>
#include <SpoolData.h>
#include <WebServerAPI.h>
#include <WebSite.h>

// #ifdef RGB_BUILTIN
// #include <FS.h>
// #include <LittleFS.h>
// #include <FastLED.h>
// #endif

// in main.cpp
extern ESPNetwork espNetwork;
extern EventHandler eventHandler;
extern WebServerAPI webServerAPI;
extern WebSite webSite;
extern RFID rfid;

// Allow serial logging for App
#ifdef MYCILA_LOGGER_SUPPORT_APP
  #include <MycilaLogger.h>
extern Mycila::Logger* serialLogger;
  #define LOGD(tag, format, ...) serialLogger->debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) serialLogger->info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) serialLogger->warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) serialLogger->error(tag, format, ##__VA_ARGS__)
#endif

// Allow logging for App via webSerial
#ifdef MYCILA_WEBSERIAL_SUPPORT_APP
  #include <MycilaLogger.h>
  #include <MycilaWebSerial.h>
extern Mycila::Logger* webLogger;
extern WebSerial webSerial;
  #define LOGD(tag, format, ...) \
    if (webLogger != nullptr)    \
    webLogger->debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) \
    if (webLogger != nullptr)    \
    webLogger->info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) \
    if (webLogger != nullptr)    \
    webLogger->warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) \
    if (webLogger != nullptr)    \
    webLogger->error(tag, format, ##__VA_ARGS__)
#endif

#if !defined(MYCILA_WEBSERIAL_SUPPORT_APP) && !defined(MYCILA_LOGGER_SUPPORT_APP)
  #define LOGD(tag, format, ...)
  #define LOGI(tag, format, ...)
  #define LOGW(tag, format, ...)
  #define LOGE(tag, format, ...)
#endif

#if defined(MYCILA_WEBSERIAL_SUPPORT_APP) && defined(MYCILA_LOGGER_SUPPORT_APP)
  #error Not supported feature set: Use either webserial or serial (or none) for logging
#endif
