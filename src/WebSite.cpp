// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <esp_heap_caps.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <Preferences.h>
#include <thingy.h>

#include <string>

#define TAG "WebSite"

#ifndef WSL_MAX_WS_CLIENTS
  #define WSL_MAX_WS_CLIENTS DEFAULT_MAX_WS_CLIENTS
#endif

// gzipped website
extern const uint8_t thingy_html_start[] asm("_binary__pio_embed_website_html_gz_start");
extern const uint8_t thingy_html_end[] asm("_binary__pio_embed_website_html_gz_end");

// constants from build process
extern const char* __COMPILED_BUILD_BOARD__;
extern const char* __COMPILED_BUILD_NAME__;
extern const char* __COMPILED_APP_VERSION__;

void WebSite::begin(Scheduler* scheduler) {
  // Task handling
  _scheduler = scheduler;
  _sr.setWaiting();

  // create and run a task for setting up the website
  Task* webSiteTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _webSiteCallback(); }, _scheduler, false, NULL, NULL, true);
  webSiteTask->enable();
  webSiteTask->waitFor(webServerAPI.getStatusRequest());
}

void WebSite::end() {
  _spooldataCallback = nullptr;
  _sr.setWaiting();

  // end the cleanup task
  if (_wsCleanupTask != nullptr) {
    _wsCleanupTask->disable();
    _wsCleanupTask = nullptr;
  }

  // delete websock handler
  if (_ws != nullptr) {
    _webServer->removeHandler(_ws);
    delete _ws;
    _ws = nullptr;
  }

#ifdef MYCILA_WEBSERIAL_SUPPORT_APP
  webSerial.end();
  delete webLogger;
#endif
}

// Add Handlers to the webserver
void WebSite::_webSiteCallback() {
  LOGD(TAG, "Starting WebSite...");

  LOGD(TAG, "Get persistent options from preferences...");
  Preferences preferences;
  preferences.begin("k2rfid", true);
  _beepOnRW = preferences.getBool("beep", false);
  _cloneSerial = preferences.getBool("clone", false);
  preferences.end();

  // configure rfid module to beep or not to beep
  rfid.enableBeep(_beepOnRW);

  // Allow web-logging for K2RFID-app via WebSerial
#ifdef MYCILA_WEBSERIAL_SUPPORT_APP
  webSerial.begin(_webServer, "/weblog", _scheduler);
  webSerial.setBuffer(100);
  webLogger = new Mycila::Logger();
  webLogger->setLevel(ARDUHAL_LOG_LEVEL_INFO);
  webLogger->forwardTo(&webSerial);
#endif

  // create websock handler
  _ws = new AsyncWebSocket("/ws");

  _ws->onEvent([&](__unused AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, __unused size_t len) -> void {
    if (type == WS_EVT_CONNECT) {
      client->keepAlivePeriod(10);
      client->setCloseClientOnQueueFull(false);

      // send ID, config, spooldata, arming,...
      JsonDocument jsonMsg;
      jsonMsg["type"] = "initial_config";
      jsonMsg["id"] = client->id();
      jsonMsg["host"] = espNetwork.getESPConnect()->getIPAddress().toString().c_str();
      jsonMsg["cloneSerial"] = _cloneSerial;
      jsonMsg["lowTxPower"] = espNetwork.getLowTxPower();
#ifdef USE_BEEPER
      jsonMsg["beepAvailable"] = true;
      jsonMsg["beepOnRW"] = _beepOnRW;
#else
      jsonMsg["beepAvailable"] = false;
      jsonMsg["beepOnRW"] = false;
#endif
      jsonMsg["writeTags"] = rfid.getWriteEnabled();
      jsonMsg["writeEmptyTags"] = !rfid.getOverwriteEnabled();
      jsonMsg["PN532"] = rfid.getStatus();

      // append spooldata - from RFID - only when length is available
      JsonDocument jsonSpool = static_cast<JsonDocument>(rfid.getSpooldata());
      if (jsonSpool["length"].as<const uint32_t>() > 0) {
        jsonMsg["spooldata"] = jsonSpool;
      }

      // send welcome message
      AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
      serializeJson(jsonMsg, buffer->get(), buffer->length());
      client->text(buffer);
      LOGD(TAG, "Client %d connected", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      LOGD(TAG, "Client %d disconnected", client->id());
    } else if (type == WS_EVT_ERROR) {
      LOGD(TAG, "Client %d error", client->id());
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len) {
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
        }
        // pong on client keep-alive message
        if (strcmp(reinterpret_cast<char*>(data), "ping") == 0) {
          // LOGD(TAG, "Client %d pinged us", client->id());
          client->text("pong");
        } else { // some message is received
          JsonDocument jsonRXMsg;
          DeserializationError error = deserializeJson(jsonRXMsg, reinterpret_cast<char*>(data));
          if (error == DeserializationError::Ok) {
            // Which kind of message was received?
            if (strcmp(jsonRXMsg["type"].as<const char*>(), "arm_state") == 0) {
              JsonDocument jsonMsg;
              jsonMsg["type"] = "arm_state";
              jsonMsg["origin"] = client->id();
              bool write = jsonRXMsg["writeTags"].as<const bool>();
              bool writeEmpty = jsonRXMsg["writeEmptyTags"].as<const bool>();
              if (writeEmpty == rfid.getOverwriteEnabled()) {
                // save persistent option in preferences
                Preferences preferences;
                preferences.begin("k2rfid", false);
                preferences.putBool("overwrite", !writeEmpty);
                preferences.end();
              }

              // for safety, disable writing first
              if (rfid.getWriteEnabled()) {
                rfid.enableWriting(false, !writeEmpty);
              }

              // was spooldata received as well?
              JsonDocument jsonSpool = jsonRXMsg["spooldata"];
              if (!jsonSpool.isNull()) {
                if (_spooldataCallback != nullptr) {
                  try {
                    _spooldataCallback(jsonSpool);
                    jsonMsg["spooldata"] = jsonSpool;
                  } catch (...) {
                    // don't try to write in case of errors
                    write = false;
                    LOGE(TAG, "ERROR while parsing send spooldata");
                  }
                }
              } else {
                // don't try to write without spooldatat
                write = false;
              }

              // configure programmer
              rfid.enableWriting(write, !writeEmpty);

              // log the new state
              if (write && !writeEmpty) {
                LOGI(TAG, "writing and re-writing enabled");
              } else if (write && writeEmpty) {
                LOGI(TAG, "writing (on empty tags) enabled");
              } else if (!write && !writeEmpty) {
                LOGI(TAG, "writing disabled (but overwrite is still set)");
              } else if (!write && writeEmpty) {
                LOGI(TAG, "writing disabled");
              }

              // fill remaining fields of the response and send
              jsonMsg["writeTags"] = write;
              jsonMsg["writeEmptyTags"] = writeEmpty;
              AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
              serializeJson(jsonMsg, buffer->get(), buffer->length());
              _ws->textAll(buffer);
            } else if (strcmp(jsonRXMsg["type"].as<const char*>(), "update_config") == 0) {
              _beepOnRW = jsonRXMsg["beepOnRW"].as<const bool>();
              rfid.enableBeep(_beepOnRW);
              _cloneSerial = jsonRXMsg["cloneSerial"].as<const bool>();
              if (jsonRXMsg["lowTxPower"].is<bool>() && jsonRXMsg["lowTxPower"].as<bool>() != espNetwork.getLowTxPower())
                espNetwork.setLowTxPower(jsonRXMsg["lowTxPower"].as<bool>());

              // save persistent options in preferences
              Preferences preferences;
              preferences.begin("k2rfid", false);
              preferences.putBool("beep", _beepOnRW);
              preferences.putBool("clone", _cloneSerial);
              preferences.end();

              // echo the config message to all connected clients
              JsonDocument jsonMsg;
              jsonMsg["type"] = "update_config";
              jsonMsg["origin"] = client->id();
              jsonMsg["beepOnRW"] = _beepOnRW;
#ifdef USE_BEEPER
              jsonMsg["beepAvailable"] = true;
#else
              jsonMsg["beepAvailable"] = false;
#endif
              jsonMsg["cloneSerial"] = _cloneSerial;
              jsonMsg["lowTxPower"] = espNetwork.getLowTxPower();
              AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
              serializeJson(jsonMsg, buffer->get(), buffer->length());
              _ws->textAll(buffer);
            }
          }
        }
      }
    }
  });

  _webServer->addHandler(_ws);

  // printer for fetching the material database via SSH: address and root password
  _webServer->on("/api/printer", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["host"] = printerDb.getHost();
    doc["customPassword"] = printerDb.hasCustomPassword();
    doc["busy"] = printerDb.isBusy();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  // save printer settings (form fields: host, password - an empty password restores the default)
  _webServer->on("/api/printer", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("host", true)) {
      printerDb.setHost(request->getParam("host", true)->value().c_str());
    }
    if (request->hasParam("password", true)) {
      printerDb.setPassword(request->getParam("password", true)->value().c_str());
    }
    LOGI(TAG, "Printer set to %s", printerDb.getHost().c_str());
    request->send(200, "text/plain", "OK");
  });

  // fetch the material database from the printer (in the background, result via websocket)
  _webServer->on("/api/fetchdb", HTTP_POST, [&](AsyncWebServerRequest* request) {
    if (printerDb.getHost().empty()) {
      return request->send(400, "text/plain", "No printer configured");
    }
    bool started = printerDb.fetch([&](bool success, const std::string& message) {
      JsonDocument jsonMsg;
      jsonMsg["type"] = "fetch_db";
      jsonMsg["result"] = success;
      jsonMsg["message"] = message;
      _textAll(jsonMsg);
      if (success) {
        JsonDocument newDbMsg;
        newDbMsg["type"] = "new_db";
        _textAll(newDbMsg);
      }
    });
    if (!started) {
      return request->send(409, "text/plain", "Already fetching the database");
    }
    request->send(202, "text/plain", "Fetching database...");
  });

  // Handle posting database
  _webServer->on(
    "/material_database.json",
    HTTP_POST,
    [&](AsyncWebServerRequest* request) {
      if (request->getResponse()) {
        return;
      }

      // check that writing is actually finished
      if (request->_tempObject == nullptr)
        return request->send(400, "text/plain", "Nothing uploaded");
      if (!(*reinterpret_cast<bool*>(request->_tempObject)))
        return request->send(400, "text/plain", "Only partial upload");

      // inform clients...
      JsonDocument jsonMsg;
      jsonMsg["type"] = "new_db";
      AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
      serializeJson(jsonMsg, buffer->get(), buffer->length());
      _ws->textAll(buffer);

      request->send(200, "text/plain", "OK");
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        // remember that we started writing
        request->_tempObject = new bool(false);

        // open file
        request->_tempFile = LittleFS.open("/material_database.json.gz", "w+");

        if (!request->_tempFile)
          request->abort();
      }

      if (len) {
        if (request->_tempObject == nullptr)
          return request->abort();

        // write content into file
        request->_tempFile.write(data, len);
      }

      if (final) {
        if (request->_tempObject == nullptr)
          return request->abort();

        // close file
        request->_tempFile.close();

        // note that writing is done
        (*reinterpret_cast<bool*>(request->_tempObject)) = true;
      }
    });

  // serve boardname info
  _webServer->on("/boardname", HTTP_GET, [](AsyncWebServerRequest* request) {
              // LOGD(TAG, "Serve boardname");
              auto* response = request->beginResponse(200, "text/plain", __COMPILED_BUILD_BOARD__);
              request->send(response);
            })
    .setFilter([](__unused AsyncWebServerRequest* request) {
      return eventHandler.getNetworkState() != Mycila::ESPConnect::State::PORTAL_STARTED;
    });

  // serve chip and memory info (for diagnosing memory problems)
  _webServer->on("/api/system/info", HTTP_GET, [](AsyncWebServerRequest* request) {
              JsonDocument doc;
              doc["chipModel"] = ESP.getChipModel();
              doc["chipRevision"] = ESP.getChipRevision();
              doc["chipCores"] = ESP.getChipCores();
              doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
              doc["flashSize"] = ESP.getFlashChipSize();
              doc["psramSize"] = ESP.getPsramSize();
              doc["psramFree"] = ESP.getFreePsram();
              doc["heapSize"] = ESP.getHeapSize();
              doc["heapFree"] = ESP.getFreeHeap();
              doc["heapMinFree"] = ESP.getMinFreeHeap();
              doc["heapMaxAlloc"] = ESP.getMaxAllocHeap();
              doc["internalFree"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
              doc["internalMaxAlloc"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
              doc["internalMinFree"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
              doc["sshStackSize"] = PRINTER_SSH_STACK_SIZE;
              doc["sshStackReserved"] = printerDb.hasReservedStack();
              doc["uptime"] = millis() / 1000;
              // for the firmware update in the web app: which release asset matches this build
              doc["version"] = __COMPILED_APP_VERSION__;
              doc["buildName"] = __COMPILED_BUILD_NAME__;
#ifdef PRINTER_SSH_STATIC_STACK
              doc["staticStack"] = true;
#else
              doc["staticStack"] = false;
#endif
              AsyncResponseStream* response = request->beginResponseStream("application/json");
              response->addHeader("Cache-Control", "no-store");
              serializeJson(doc, *response);
              request->send(response);
            })
    .setFilter([](__unused AsyncWebServerRequest* request) {
      return eventHandler.getNetworkState() != Mycila::ESPConnect::State::PORTAL_STARTED;
    });

  // serve app-version info
  _webServer->on("/appversion", HTTP_GET, [](AsyncWebServerRequest* request) {
              // LOGD(TAG, "Serve appversion");
              auto* response = request->beginResponse(200, "text/plain", APP_VERSION);
              request->send(response);
            })
    .setFilter([](__unused AsyncWebServerRequest* request) {
      return eventHandler.getNetworkState() != Mycila::ESPConnect::State::PORTAL_STARTED;
    });

  // serve our home page here, yet only when the ESPConnect portal is not shown
  _webServer->on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
              // LOGD(TAG, "Serve...");
              auto* response = request->beginResponse(200,
                                                      "text/html",
                                                      thingy_html_start,
                                                      thingy_html_end - thingy_html_start);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            })
    .setFilter([](__unused AsyncWebServerRequest* request) {
      return eventHandler.getNetworkState() != Mycila::ESPConnect::State::PORTAL_STARTED;
    });

  // register event handlers to reader
  LOGD(TAG, "register event handlers to reader");
  rfid.listenTagRead([&](CFSTag tag) { _tagReadCallback(tag); });
  rfid.listenTagWrite([&](bool success) { _tagWriteCallback(success); });

  // set up a task to cleanup orphan websock-clients
  _disconnectTime = millis();
  Task* _wsCleanupTask = new Task(1000, TASK_FOREVER, [&] { _wsCleanupCallback(); }, _scheduler, false, NULL, NULL, true);
  _wsCleanupTask->enable();

  _sr.signalComplete();
  LOGD(TAG, "...done!");
}

// send a json message to all connected websocket clients
void WebSite::_textAll(const JsonDocument& jsonMsg) {
  if (_ws == nullptr || !_ws->count())
    return;
  AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
  serializeJson(jsonMsg, buffer->get(), buffer->length());
  _ws->textAll(buffer);
}

// get StatusRequest object for initializing task
StatusRequest* WebSite::getStatusRequest() {
  return &_sr;
}

// Handle spooldata from reader received event
void WebSite::_tagReadCallback(CFSTag tag) {
  if (tag.isEmpty()) {
    JsonDocument jsonMsg;
    jsonMsg["type"] = "read_tag";
    jsonMsg["uid"] = static_cast<std::string>(tag.getUid()).c_str();
    AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
    serializeJson(jsonMsg, buffer->get(), buffer->length());
    if (_ws->count()) {
      _ws->textAll(buffer);
    }
  } else {
    SpoolData spooldata = tag.getSpooldata();
    JsonDocument jsonMsg;
    jsonMsg["type"] = "read_spool";
    jsonMsg["uid"] = static_cast<std::string>(tag.getUid()).c_str();
    jsonMsg["spooldata"] = static_cast<JsonDocument>(tag.getSpooldata());
    AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
    serializeJson(jsonMsg, buffer->get(), buffer->length());
    if (_ws->count()) {
      _ws->textAll(buffer);
    }
  }
}

// Handle spooldata written by reader event
void WebSite::_tagWriteCallback(bool success) {
  LOGD(TAG, "Spooldata written %s", success ? "sucessfully" : "unsucessfully");
  JsonDocument jsonMsg;
  jsonMsg["type"] = "write_spool";
  jsonMsg["result"] = success;
  AsyncWebSocketMessageBuffer* buffer = new AsyncWebSocketMessageBuffer(measureJson(jsonMsg));
  serializeJson(jsonMsg, buffer->get(), buffer->length());
  if (_ws->count()) {
    _ws->textAll(buffer);
  }
}

void WebSite::_wsCleanupCallback() {
  _ws->cleanupClients(WSL_MAX_WS_CLIENTS);
}
