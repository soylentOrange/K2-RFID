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
  _tagReadErrorCallback = nullptr;
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
    // a tag was found, so start counting missed cycles from scratch
    _retryCounter = RETRIES;

    // new tag is foud
    if (_lastTag != tag) {
      _lastTag = tag;
      _tagInProximity = true;
      _newTagInProximity = true;
      std::string uid = static_cast<std::string>(tag.getUid());
      // a tag that couldn't be read in the last cycle is just read again
      bool retryingRead = (_readError > 0) && (_failedReadUid == uid);
      if (!retryingRead) {
        _readError = 0;
        _failedReadUid.clear();
        if (tag._encrypted) {
          LOGI(TAG, "encrypted tag (%s) found...", uid.c_str());
        } else {
          LOGI(TAG, "un-encrypted tag (%s) found...", uid.c_str());
        }
      }

      // read spooldata from tag
      bool readOk = tag.readSpoolData(&_nfc);
      // a tag where a previous write attempt failed (possibly leaving partial
      // data on it) is written again, even when overwriting isn't enabled
      bool retryFailedWrite = !_failedWriteUid.empty() && (_failedWriteUid == uid);
      // is the tag going to be written regardless of its content?
      bool writeAnyway = _writeEnabled && (_overwriteEnabled || retryFailedWrite);

      if (!readOk && !writeAnyway) {
        if (tag.hadReadError() && (++_readError <= READ_RETRIES)) {
          // the reader couldn't read the tag (e.g. because it was just entering
          // or leaving the reader's field): forget the tag, so it'll be
          // selected, authenticated and read again in the next cycle
          LOGW(TAG, "reading tag (%s) failed (attempt %u), retrying...", uid.c_str(), _readError);
          _failedReadUid = uid;
          _lastTag = CFSTag();
          _tagInProximity = false;
          _newTagInProximity = false;
          return;
        }
        // give up; keep the tag as last tag, so it is read again only after it
        // was removed and presented again
        _signalReadError(uid, !tag.hadReadError());
        return;
      }
      _readError = 0;
      _failedReadUid.clear();

      if (!readOk) {
        LOGW(TAG, "tag data corrupted or reader error...");
      } else if (tag.isEmpty()) {
        LOGD(TAG, "tag is empty...");
      } else {
        LOGD(TAG, "tag is not empty...");
      }

      if (_writeEnabled && ((readOk && tag.isEmpty()) || writeAnyway)) {
        // write tag
        _writeTag(tag, readOk && tag.isEmpty());
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
      // the tag was removed while it was still being read again
      if (_readError > 0) {
        _signalReadError(_failedReadUid, false);
      }
      _tagInProximity = false;
      _newTagInProximity = false;
    }
  }
}

// signal a tag that couldn't be read
void RFID::_signalReadError(const std::string& uid, bool invalidData) {
  if (invalidData) {
    LOGW(TAG, "tag (%s) doesn't hold valid spooldata", uid.c_str());
  } else {
    LOGW(TAG, "reading tag (%s) failed, hold the tag still on the reader and present it again", uid.c_str());
  }
  _readError = 0;
  _failedReadUid.clear();
  led.setMode(LED::LEDMode::TAG_ERROR);
  _doBeep(3000);
  // invoke event callback
  if (_tagReadErrorCallback != nullptr) {
    _tagReadErrorCallback(uid, invalidData);
  }
}

// write spooldata onto the tag and signal the (verified) result
void RFID::_writeTag(CFSTag& tag, bool wasEmpty) {
  std::string uid = static_cast<std::string>(tag.getUid());
  LOGW(TAG, "%s tag (%s)...", wasEmpty ? "writing empty" : "re-writing", uid.c_str());

  // writeSpoolData only returns true after the written data was read back and verified
  if (tag.writeSpoolData(&_nfc, _spooldata)) {
    _lastTag = tag;
    _writeError = 0;
    _failedWriteUid.clear();
    led.setMode(wasEmpty ? LED::LEDMode::TAG_WRITTEN : LED::LEDMode::TAG_REWRITTEN);
    _doBeep(2000);
    // invoke event callback
    if (_tagWriteCallback != nullptr) {
      _tagWriteCallback(true);
    }
    return;
  }

  // writing failed: remember the tag, so it'll be written again on the next try
  LOGW(TAG, "writing tag (%s) failed (attempt %u)", uid.c_str(), _writeError + 1);
  _failedWriteUid = uid;
  if (++_writeError > WRITE_RETRIES) {
    // give up for now; keep the tag as last tag, so it is retried only
    // after it was removed and presented again
    _writeError = 0;
    led.setMode(LED::LEDMode::ERROR);
    _doBeep(3000);
    // invoke event callback
    if (_tagWriteCallback != nullptr) {
      _tagWriteCallback(false);
    }
  } else {
    // forget the tag, so it'll be detected (and written) again in the next cycle
    _tagInProximity = false;
    _newTagInProximity = false;
    _lastTag = CFSTag();
  }
}

// enable writing tag with the provided SpoolData
void RFID::enableWriting(bool enable, bool overwrite) {
  _writeEnabled = enable;
  _overwriteEnabled = overwrite;
  _writeError = 0;
  _failedWriteUid.clear();

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
