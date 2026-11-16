// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <Preferences.h>
#include <libssh_esp32.h>
#include <thingy.h>

#include <libssh/libssh.h>

#define TAG "PrinterDb"

#define DB_FILE      "/material_database.json.gz"
#define DB_TEMP_FILE "/material_database.json.gz.tmp"
#define DB_MAX_SIZE  (128 * 1024) // gzipped; the printer's database is ~25 KB

static const char* FETCH_COMMAND = "gzip -c " PRINTER_DB_PATH;

// heap state, for diagnosing memory problems on boards with/without PSRAM
static std::string memoryInfo() {
  char info[160];
  snprintf(info, sizeof(info), "free heap %u KB, largest block %u KB, PSRAM %u KB (%u KB free)",
           static_cast<unsigned>(ESP.getFreeHeap() / 1024), static_cast<unsigned>(ESP.getMaxAllocHeap() / 1024),
           static_cast<unsigned>(ESP.getPsramSize() / 1024), static_cast<unsigned>(ESP.getFreePsram() / 1024));
  return info;
}

void PrinterDb::begin(Scheduler* scheduler) {
  _scheduler = scheduler;

  Preferences preferences;
  preferences.begin("k2rfid", true);
  _host = preferences.getString("printerHost", "").c_str();
  _password = preferences.getString("printerPass", PRINTER_DEFAULT_PASSWORD).c_str();
  preferences.end();

  // report the result of a background fetch from within the scheduler (not the ssh task)
  Task* monitorTask = new Task(250, TASK_FOREVER, [&] { _monitorCallback(); }, _scheduler, false, NULL, NULL, true);
  monitorTask->enable();
}

void PrinterDb::setHost(const std::string& host) {
  _host = host;
  Preferences preferences;
  preferences.begin("k2rfid", false);
  preferences.putString("printerHost", _host.c_str());
  preferences.end();
}

void PrinterDb::setPassword(const std::string& password) {
  _password = password.empty() ? PRINTER_DEFAULT_PASSWORD : password;
  Preferences preferences;
  preferences.begin("k2rfid", false);
  preferences.putString("printerPass", _password.c_str());
  preferences.end();
}

bool PrinterDb::fetch(ResultCallback callback) {
  if (_host.empty())
    return false;

  State expected = State::IDLE;
  if (!_state.compare_exchange_strong(expected, State::RUNNING))
    return false;

  _callback = callback;
  _fetchHost = _host;
  _fetchPassword = _password;

  // libssh is blocking and needs a big stack - give it a task of its own
  if (xTaskCreate(_fetchTask, "printerDb", PRINTER_SSH_STACK_SIZE, this, 1, nullptr) != pdPASS) {
    _success = false;
    _message = "Not enough memory to start the download (" + memoryInfo() + ", needs " + std::to_string(PRINTER_SSH_STACK_SIZE / 1024) + " KB in one block)";
    _state = State::DONE;
  }
  return true;
}

void PrinterDb::_fetchTask(void* arg) {
  PrinterDb* self = static_cast<PrinterDb*>(arg);
  std::string message;
  self->_success = self->_fetchViaSSH(message);
  self->_message = message;
  self->_state = State::DONE;
  vTaskDelete(nullptr);
}

void PrinterDb::_monitorCallback() {
  if (_state != State::DONE)
    return;

  if (_success) {
    LOGI(TAG, "%s", _message.c_str());
  } else {
    LOGE(TAG, "%s", _message.c_str());
  }
  ResultCallback callback = _callback;
  _callback = nullptr;
  _state = State::IDLE;
  if (callback != nullptr)
    callback(_success, _message);
}

// authenticate as root, with password or keyboard-interactive (answering every prompt with the password)
static bool authenticate(ssh_session session, const char* password) {
  if (ssh_userauth_password(session, nullptr, password) == SSH_AUTH_SUCCESS)
    return true;

  int rc = ssh_userauth_kbdint(session, nullptr, nullptr);
  while (rc == SSH_AUTH_INFO) {
    int prompts = ssh_userauth_kbdint_getnprompts(session);
    for (int i = 0; i < prompts; i++) {
      ssh_userauth_kbdint_setanswer(session, i, password);
    }
    rc = ssh_userauth_kbdint(session, nullptr, nullptr);
  }
  return rc == SSH_AUTH_SUCCESS;
}

bool PrinterDb::_fetchViaSSH(std::string& message) {
  if (!_sshInitialized) {
    libssh_begin();
    _sshInitialized = true;
  }

  LOGI(TAG, "Fetching material database from %s via SSH (%s)...", _fetchHost.c_str(), memoryInfo().c_str());
  bool success = false;
  ssh_channel channel = nullptr;
  File file;

  ssh_session session = ssh_new();
  if (session == nullptr) {
    message = "Not enough memory for SSH (" + memoryInfo() + ")";
    return false;
  }

  long timeout = PRINTER_SSH_TIMEOUT;
  int port = 22;
  ssh_options_set(session, SSH_OPTIONS_HOST, _fetchHost.c_str());
  ssh_options_set(session, SSH_OPTIONS_PORT, &port);
  ssh_options_set(session, SSH_OPTIONS_USER, "root");
  ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);

  do {
    if (ssh_connect(session) != SSH_OK) {
      message = std::string("Can't connect to the printer at ") + _fetchHost + ": " + ssh_get_error(session);
      break;
    }

    // Note: the printer's host key is not verified (like ssh -o StrictHostKeyChecking=no)
    if (!authenticate(session, _fetchPassword.c_str())) {
      message = "Login to the printer failed - check the root password";
      break;
    }

    channel = ssh_channel_new(session);
    if (channel == nullptr || ssh_channel_open_session(channel) != SSH_OK || ssh_channel_request_exec(channel, FETCH_COMMAND) != SSH_OK) {
      message = std::string("Can't run command on the printer: ") + ssh_get_error(session);
      break;
    }

    file = LittleFS.open(DB_TEMP_FILE, "w");
    if (!file) {
      message = "Can't write to the file system";
      break;
    }

    // stream the gzipped database into the file
    uint8_t buffer[512];
    size_t total = 0;
    std::string readError;
    while (true) {
      int n = ssh_channel_read_timeout(channel, buffer, sizeof(buffer), 0, PRINTER_SSH_TIMEOUT * 1000);
      if (n == 0 && ssh_channel_is_eof(channel))
        break; // done
      if (n == 0) {
        readError = "no data from the printer for " + std::to_string(PRINTER_SSH_TIMEOUT) + " s";
        break;
      }
      if (n < 0) {
        readError = std::string("SSH error: ") + ssh_get_error(session);
        break;
      }
      total += n;
      if (total > DB_MAX_SIZE) {
        readError = "database is larger than " + std::to_string(DB_MAX_SIZE / 1024) + " KB";
        break;
      }
      if (file.write(buffer, n) != static_cast<size_t>(n)) {
        readError = "can't write to the file system (full?)";
        break;
      }
    }
    file.close();

    uint32_t exitCode = 0;
    ssh_channel_send_eof(channel);
    ssh_channel_get_exit_state(channel, &exitCode, nullptr, nullptr);

    if (!readError.empty()) {
      message = "Error while downloading the database after " + std::to_string(total) + " bytes: " + readError + " (" + memoryInfo() + ")";
      break;
    }
    if (exitCode != 0 || total < 20) {
      message = "The printer has no material database at " PRINTER_DB_PATH;
      break;
    }

    // check gzip magic, then replace the current database
    file = LittleFS.open(DB_TEMP_FILE, "r");
    bool isGzip = file && file.read() == 0x1f && file.read() == 0x8b;
    file.close();
    if (!isGzip) {
      message = "Unexpected data received from the printer";
      break;
    }
    LittleFS.remove(DB_FILE);
    if (!LittleFS.rename(DB_TEMP_FILE, DB_FILE)) {
      message = "Can't save the database";
      break;
    }

    message = std::string("Fetched material database from ") + _fetchHost + " (" + std::to_string(total) + " bytes)";
    success = true;
  } while (false);

  if (LittleFS.exists(DB_TEMP_FILE))
    LittleFS.remove(DB_TEMP_FILE);
  if (channel != nullptr) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
  }
  ssh_disconnect(session);
  ssh_free(session);
  return success;
}
