// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <Adafruit_PN532.h>
#include <Preferences.h>
#include <thingy.h>

#include <iostream>
#include <string>
using std::runtime_error;

#define TAG "RFID"

void RFID::begin(Scheduler* scheduler) {
  // Task handling
  _scheduler = scheduler;

  // create and run a task for initializing the nfc device
  Task* initNFCTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initNFCcallback(); }, _scheduler, false, NULL, NULL, true);
  initNFCTask->enable();
  initNFCTask->waitFor(webSite.getStatusRequest());

  // Register listener to website
  webSite.listenSpooldata([&](JsonDocument doc) { _spooldataRxCallback(doc); });
}

void RFID::end() {
  LOGD(TAG, "Stopping RFID...");
  _PN532Status = false;
  if (_rfidReadTask != nullptr) {
    _rfidReadTask->disable();
  }
  if (_rfidWriteTask != nullptr) {
    _rfidWriteTask->disable();
  }
  _tagReadCallback = nullptr;
  _tagWriteCallback = nullptr;
  LOGD(TAG, "...done!");
}

void RFID::_initNFCcallback() {
  // possibly delay initialization if network isn't connected to WiFi
  // like programming...
  if (eventHandler.getStatusRequest()->pending()) {
    LOGI(TAG, "Delay RFID setup");
    Task* initDelayedNFCTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initNFCcallback(); }, _scheduler, false, NULL, NULL, true);
    initDelayedNFCTask->enable();
    initDelayedNFCTask->waitFor(eventHandler.getStatusRequest());
    return;
  }

  LOGD(TAG, "Starting RFID...");
  _spi->begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
  _nfc.begin();

  uint32_t versiondata = _nfc.getFirmwareVersion();
  if (!versiondata) {
    LOGE(TAG, "Didn't find PN53x board");

    // Retry initialization
    Task* initNFCTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initNFCcallback(); }, _scheduler, false, NULL, NULL, true);
    initNFCTask->enableDelayed(250);
  } else {
    // Got ok data, print it out!
    LOGD(TAG, "Found chip PN5%x", (versiondata >> 24) & 0xFF);
    LOGD(TAG, "Firmware ver. %d.%d", (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);
    LOGD(TAG, "...done!");

    // get some preference for writing behaviour
    Preferences preferences;
    preferences.begin("k2rfid", true);
    _overwriteEnabled = preferences.getBool("overwrite", false);
    preferences.end();

    // start a task for continuously trying to find and read tags in proximity
    _rfidReadTask = new Task(250, TASK_FOREVER, [&] { _rfidReadCallback(); }, _scheduler, false, NULL, NULL, true);
    _rfidReadTask->enable();
    _PN532Status = true;

    // show LED state
    led.setMode(getStatus_as_LEDMode());
  }
}

// beep when requested and possible
void RFID::_doBeep(uint32_t freq) {
#ifdef USE_BEEPER
  if (_beep)
    tone(BEEPER_PIN, freq, 100);
#endif
}

// try to read tag
void RFID::_rfidReadCallback() {
  try { // check if some tag is present
    CFSTag tag(&_nfc);

    // new tag is foud
    if (_lastTag != tag) {
      _lastTag = tag;
      _tagInProximity = true;
      _newTagInProximity = true;
      if (tag._encrypted) {
        LOGI(TAG, "encrypted tag (%s) found...", static_cast<std::string>(tag.getUid()).c_str());
      } else {
        LOGI(TAG, "un-encrypted tag (%s) found...", static_cast<std::string>(tag.getUid()).c_str());
      }

      // read spooldata from tag
      if (tag.readSpoolData(&_nfc)) {
        if (tag.isEmpty()) {
          LOGD(TAG, "tag is empty...");
          // possibly write tag here
          if (_writeEnabled) {
            LOGW(TAG, "writing empty tag...");
            bool write_result = tag.writeSpoolData(&_nfc, _spooldata);
            led.setMode(LED::LEDMode::TAG_WRITTEN);
            _doBeep(2000);
            // invoke callback
            if (_tagWriteCallback != nullptr) {
              _tagWriteCallback(write_result);
            }
          } else {
            led.setMode(LED::LEDMode::TAG_READ);
            _doBeep();
            // invoke event callback
            if (_tagReadCallback != nullptr) {
              _tagReadCallback(tag);
            }
          }
        } else {
          LOGD(TAG, "tag is not empty...");
          // LOGD(TAG, "read from tag: %s", static_cast<std::string>(tag.getSpooldata()).c_str());
          // possibly write tag here
          if (_writeEnabled && _overwriteEnabled) {
            LOGW(TAG, "re-writing tag...");
            bool overwrite_result = tag.writeSpoolData(&_nfc, _spooldata);
            if (overwrite_result) {
              led.setMode(LED::LEDMode::TAG_REWRITTEN);
              _doBeep(2000);
              _writeError = 0;
              // invoke event callback
              if (_tagWriteCallback != nullptr) {
                _tagWriteCallback(overwrite_result);
              }
            } else {
              _tagInProximity = false;
              _newTagInProximity = false;
              _lastTag = CFSTag();
              if (++_writeError > 10) {
                // invoke event callback
                if (_tagWriteCallback != nullptr) {
                  _tagWriteCallback(overwrite_result);
                }
                led.setMode(LED::LEDMode::ERROR);
                _doBeep(3000);
              }
            }
          } else {
            // Only signal when writing isn't enabled
            if (!_writeEnabled) {
              led.setMode(LED::LEDMode::TAG_READ);
              _doBeep();
            }
            // invoke event callback
            if (_tagReadCallback != nullptr) {
              _tagReadCallback(tag);
            }
          }
        }
      } else {
        LOGW(TAG, "tag data corrupted or reader error...");
        // possibly write tag here
        if (_writeEnabled && _overwriteEnabled) {
          LOGW(TAG, "writing corrupted tag...");
          bool write_result = tag.writeSpoolData(&_nfc, _spooldata);
          if (write_result) {
            led.setMode(LED::LEDMode::TAG_REWRITTEN);
            _doBeep(2000);
            _writeError = 0;
            // invoke callback
            if (_tagWriteCallback != nullptr) {
              _tagWriteCallback(write_result);
            }
          } else {
            _tagInProximity = false;
            _newTagInProximity = false;
            _lastTag = CFSTag();
            if (++_writeError > 10) {
              // invoke event callback
              if (_tagWriteCallback != nullptr) {
                _tagWriteCallback(write_result);
              }
              led.setMode(LED::LEDMode::ERROR);
              _doBeep(3000);
            }
          }
        } else {
          led.setMode(LED::LEDMode::TAG_READ);
          _doBeep();
          // invoke event callback
          if (_tagReadCallback != nullptr) {
            _tagReadCallback(tag);
          }
        }
      }
    } else { // the last tag is still in proximity
      _tagInProximity = true;
      _newTagInProximity = false;
    }
  } catch (const runtime_error& e) { // no tag in proximity
    if (!(--_retryCounter)) {        // try it a few times before accepting that the tag is really gone
      _retryCounter = RETRIES;
      if (_lastTag != CFSTag()) { // well, it seems to be really gone
        LOGI(TAG, "tag (%s) is gone...\n", static_cast<std::string>(_lastTag.getUid()).c_str());
        _lastTag = CFSTag();
      }
      _tagInProximity = false;
      _newTagInProximity = false;
    }
  }
}

// enable writing tag with the provided SpoolData
void RFID::enableWriting(bool enable, bool overwrite) {
  _writeEnabled = enable;
  _overwriteEnabled = overwrite;
  _writeError = 0;

  if (_writeEnabled && _overwriteEnabled) {
    led.setMode(LED::LEDMode::ARMED_REWRITING);
  } else if (_writeEnabled && !_overwriteEnabled) {
    led.setMode(LED::LEDMode::ARMED_WRITING);
  } else if (!_writeEnabled && _overwriteEnabled) {
    led.setMode(LED::LEDMode::WAITING_READ);
  } else {
    led.setMode(LED::LEDMode::WAITING_READ);
  }
}

// enable writing tag with the provided SpoolData
void RFID::enableBeep(bool enable) {
  _beep = enable;
}

// Handle spooldata from app received event
void RFID::_spooldataRxCallback(JsonDocument doc) {
  // save it for writing
  _spooldata = SpoolData(doc);
  LOGI(TAG, "Spooldata received for writing: %s", static_cast<std::string>(_spooldata).c_str());
}
