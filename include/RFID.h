// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <Adafruit_PN532.h>
#include <CFSTag.h>
#include <SPI.h>
#include <SpoolData.h>
#include <TaskSchedulerDeclarations.h>

#include <string>

#define RETRIES 3

class RFID {
  public:
    explicit RFID(SPIClass& spi) : _spi(&spi), _nfc(Adafruit_PN532(PN532_SS, &spi)) {}
    void begin(Scheduler* scheduler);
    void end();
    // enable writing onto tags
    void enableWriting(bool enable = true, bool overwrite = false);
    bool getWriteEnabled() { return _writeEnabled; }
    bool getOverwriteEnabled() { return _overwriteEnabled; }
    SpoolData getSpooldata() { return _spooldata; }
    typedef std::function<void(CFSTag tag)> TagReadCallback;
    void listenTagRead(TagReadCallback callback) { _tagReadCallback = callback; }
    typedef std::function<void(bool success)> TagWriteCallback;
    void listenTagWrite(TagWriteCallback callback) { _tagWriteCallback = callback; }
    void enableBeep(bool enable);

  private:
    void _rfidReadCallback();
    Task* _rfidReadTask = nullptr;
    Task* _rfidWriteTask = nullptr;
    void _initNFCcallback();
    Scheduler* _scheduler = nullptr;
    SPIClass* _spi;
    Adafruit_PN532 _nfc;
    bool _tagInProximity = false;
    bool _newTagInProximity = false;
    int32_t _retryCounter = RETRIES;
    CFSTag _lastTag = CFSTag();
    SpoolData _spooldata = SpoolData();
    void _spooldataRxCallback(JsonDocument doc);
    TagReadCallback _tagReadCallback = nullptr;
    TagWriteCallback _tagWriteCallback = nullptr;
    bool _writeEnabled = false;
    bool _overwriteEnabled = false;
    bool _beep = false;
    void _doBeep();
};
