// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2025 Mathieu Carbou, 2025 Robert Wendlandt
 */

#include <MycilaWebSerial.h>
#include <assert.h>

#include <string>

// gzipped website
extern const uint8_t webserial_html_start[] asm("_binary__pio_embed_webserial_html_gz_start");
extern const uint8_t webserial_html_end[] asm("_binary__pio_embed_webserial_html_gz_end");

void WebSerial::begin(AsyncWebServer* server, const char* url, Scheduler* scheduler) {
  _server = server;
  _scheduler = scheduler;

  std::string backendUrl = url;
  backendUrl.append("ws");
  _ws = new AsyncWebSocket(backendUrl.c_str());

  _server->on(url, HTTP_GET, [&](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", webserial_html_start, webserial_html_end - webserial_html_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  _ws->onEvent([&](__unused AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, __unused void* arg, uint8_t* data, __unused size_t len) -> void {
    if (type == WS_EVT_CONNECT) {
      client->setCloseClientOnQueueFull(false);
      client->keepAlivePeriod(10);
      return;
    }
    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len) {
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
        }
        if (strcmp(reinterpret_cast<char*>(data), "ping") == 0)
          client->text("pong");
      }
    }
  });

  _server->addHandler(_ws);

  // set up a task to cleanup orphan websock-clients
  Task* _wsCleanupTask = new Task(1000, TASK_FOREVER, [&] { _wsCleanupCallback(); }, _scheduler, false, NULL, NULL, true);
  _wsCleanupTask->enable();
}

void WebSerial::end() {
  // end the cleanup task
  if (_wsCleanupTask != nullptr) {
    _wsCleanupTask->disable();
    _wsCleanupTask = nullptr;
  }

  // delete websock handler
  if (_ws != nullptr) {
    _server->removeHandler(_ws);
    delete _ws;
    _ws = nullptr;
  }
}

size_t WebSerial::write(uint8_t m) {
  if (!_ws)
    return 0;

  // We do not support non-buffered write on webserial
  // we fail with a stack trace allowing the user to change the code to use write(const uint8_t* buffer, size_t size) instead
  if (!_initialBufferCapacity) {
#ifdef ESP8266
    ets_printf("Non-buffered write is not supported: use webSerial.setBuffer(size_t)");
#else
    log_e("Non-buffered write is not supported: use webSerial.setBuffer(size_t)");
#endif
    assert(false);
    return 0;
  }

  write(&m, 1);
  return (1);
}

size_t WebSerial::write(const uint8_t* buffer, size_t size) {
  if (!_ws || size == 0)
    return 0;

  // No buffer, send directly (i.e. use case for log streaming)
  if (!_initialBufferCapacity) {
    size = buffer[size - 1] == '\n' ? size - 1 : size;
    _send(buffer, size);
    return size;
  }

  // fill the buffer while sending data for each EOL
  size_t start = 0, end = 0;
  while (end < size) {
    if (buffer[end] == '\n') {
      if (end > start) {
        _buffer.append(reinterpret_cast<const char*>(buffer + start), end - start);
      }
      _send(reinterpret_cast<const uint8_t*>(_buffer.c_str()), _buffer.length());
      start = end + 1;
    }
    end++;
  }
  if (end > start) {
    _buffer.append(reinterpret_cast<const char*>(buffer + start), end - start);
  }
  return size;
}

void WebSerial::_send(const uint8_t* buffer, size_t size) {
  if (_ws && size > 0) {
    //_ws->cleanupClients(WSL_MAX_WS_CLIENTS);
    if (_ws->count()) {
      _ws->textAll((const char*)buffer, size);
    }
  }

  // if buffer grew too much, free it, otherwise clear it
  if (_initialBufferCapacity) {
    if (_buffer.length() > _initialBufferCapacity) {
      setBuffer(_initialBufferCapacity);
    } else {
      _buffer.clear();
    }
  }
}

void WebSerial::setBuffer(size_t initialCapacity) {
  assert(initialCapacity <= UINT16_MAX);
  _initialBufferCapacity = initialCapacity;
  _buffer = std::string();
  _buffer.reserve(initialCapacity);
}

void WebSerial::_wsCleanupCallback() {
  _ws->cleanupClients(WSL_MAX_WS_CLIENTS);
}
