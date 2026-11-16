// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

#include <atomic>
#include <functional>
#include <string>

// Fetch the material database from a Creality printer via SSH.
//
// Current K2/K2 Plus firmware doesn't serve the database via http anymore, but it can
// be read via SSH (as root). The printer compresses it (gzip), so only ~25 KB need to be
// transferred and stored - exactly the format served to the web app.
class PrinterDb {
  public:
    typedef std::function<void(bool success, const std::string& message)> ResultCallback;

    void begin(Scheduler* scheduler);

    // printer settings, persisted in preferences
    std::string getHost() const { return _host; }
    bool hasCustomPassword() const { return _password != PRINTER_DEFAULT_PASSWORD; }
    void setHost(const std::string& host);
    void setPassword(const std::string& password); // empty: back to default

    // start fetching in the background; false when busy or no printer configured
    bool fetch(ResultCallback callback);
    bool isBusy() const { return _state != State::IDLE; }
#ifdef PRINTER_SSH_STATIC_STACK
    bool hasReservedStack() const { return _staticTask != nullptr; }
#else
    bool hasReservedStack() const { return false; }
#endif

  private:
    enum class State : uint8_t { IDLE, RUNNING, DONE };
    static void _fetchTask(void* arg);
#ifdef PRINTER_SSH_STATIC_STACK
    static void _staticFetchTask(void* arg);
    TaskHandle_t _staticTask = nullptr;
#endif
    bool _fetchViaSSH(std::string& message);
    void _monitorCallback();

    Scheduler* _scheduler = nullptr;
    std::string _host;
    std::string _password = PRINTER_DEFAULT_PASSWORD;
    std::string _fetchHost;
    std::string _fetchPassword;
    std::atomic<State> _state{State::IDLE};
    bool _success = false;
    std::string _message;
    ResultCallback _callback = nullptr;
    bool _sshInitialized = false;
};
