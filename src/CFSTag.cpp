// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <thingy.h>

#include <string>
using std::runtime_error;

#define TAG "CFSTag"

// constructor from PN532 nfc reader
// beware: will throw exception in case of failure
CFSTag::CFSTag(Adafruit_PN532* nfc) {
  _uid = Uid();
  bool success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, _uid.uidByte, &_uid.size, PN532_TIMEOUT);

  // have we found a MIFARE classic tag?
  if (success && _uid.size == 4) {
    _eKey = createKey(_uid);
    bool unlocked = false;

    // try authentification with created key
    if (nfc->mifareclassic_AuthenticateBlock(_uid.uidByte, _uid.size, 7, 0, _eKey.keyByte)) {
      _encrypted = true;
      unlocked = true;
    }

    // retry authentification with standard key
    if (!_encrypted) {
      nfc->reset();
      delay(PN532_TIMEOUT);
      nfc->wakeup();
      success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, _uid.uidByte, &_uid.size, PN532_TIMEOUT);
      if (success)
        unlocked = nfc->mifareclassic_AuthenticateBlock(_uid.uidByte, _uid.size, 7, 0, const_cast<uint8_t*>(std_key.keyByte));
    }

    if (!unlocked)
      throw runtime_error("authentification failed");
  } else {
    throw runtime_error("No tag in proximity");
  }
}

static void dumpSpooldata(const CFSTag::MIFARE_tripleBlock& spooldata) {
  // prepare dump of spooldata
  std::string asciidump = "";
  std::string hexdump = "";
  char buffer[4];
  for (char const& c : spooldata.data) {
    // insert two spaces before each character
    snprintf(buffer, sizeof(buffer), "  %c", isalnum(c) ? c : '.');
    asciidump += buffer;
    // make hex form character and add one space
    snprintf(buffer, sizeof(buffer), " %02x", c);
    hexdump += buffer;
  }
  LOGI(TAG, "  Ascii:");
  LOGI(TAG, "        .0 .1 .2 .3 .4 .5 .6 .7");
  for (size_t row = 0; row < 6; ++row) {
    LOGI(TAG, "    %d. %s", row, asciidump.substr(row * 24, 24).c_str());
  }
  LOGI(TAG, "  Hex:");
  LOGI(TAG, "        .0 .1 .2 .3 .4 .5 .6 .7");
  for (size_t row = 0; row < 6; ++row) {
    LOGI(TAG, "    %d. %s", row, hexdump.substr(row * 24, 24).c_str());
  }
}

bool CFSTag::readSpoolData(Adafruit_PN532* nfc) {
  // read data from blocks 4 - 6
  MIFARE_tripleBlock encryptedData, plainData;
  uint8_t result = 0;
  for (size_t i = 0; i < 3; ++i)
    result += nfc->mifareclassic_ReadDataBlock(i + 4, encryptedData.blockData[i]);

  // return with error when reading didn't succeed
  if (result != 3) {
    LOGE(TAG, "RFID reader error");
    _empty = false;
    return false;
  }

  // possibly decrypt data from blocks 4 - 6
  if (_encrypted) {
    if (!decrypt(encryptedData, plainData)) {
      LOGE(TAG, "decryption error");
      _empty = false;
      return false;
    }
  } else {
    plainData = encryptedData;
  }

  // convert read (and decrypted) data to string
  std::string spooldataStr = plainData.data;

  // empty (yet unwritten) tag?
  if (!spooldataStr.size()) { // yes, it's empty
    LOGI(TAG, "tag without spooldata");
    return true;
  } else { // there is something on the tag
    LOGD(TAG, "Read tag with spooldata");
    _empty = false;
  }

  // cleanup data
  // just remove everything after the last non-zero numeric
  spooldataStr.erase(spooldataStr.find_last_of("123456789") + 1);

  // try to construct SpoolData from string
  try {
    _spooldata = SpoolData(spooldataStr);
  } catch (const std::exception& e) {
    _spooldata = SpoolData();
    dumpSpooldata(plainData);
    return false;
  }

  // Everything went well, spooldata is available now
  LOGI(TAG, "Spooldata read: %s", static_cast<std::string>(_spooldata).c_str());
  dumpSpooldata(plainData);
  _validMaterial = true;
  return true;
}

bool CFSTag::writeSpoolData(Adafruit_PN532* nfc, SpoolData spooldata) {
  MIFARE_tripleBlock plainData, encryptedData;

  // pad and copy spooldata into buffer
  std::string spooldataStr = static_cast<std::string>(spooldata);
  if (spooldataStr.size() < 48) { // pad with '0'
    spooldataStr.append(48 - spooldataStr.size(), '0');
  }
  memset(plainData.data, 0, 48);
  strncpy(plainData.data, spooldataStr.c_str(), 48);

  // encrypt data and check for error
  if (!encrypt(plainData, encryptedData)) {
    LOGE(TAG, "Encrypting spooldata failed");
    return false;
  }

  // write data into blocks 4 - 6
  uint8_t result = 0;
  for (size_t i = 0; i < 3; ++i)
    result += nfc->mifareclassic_WriteDataBlock(i + 4, encryptedData.blockData[i]);

  // check for error
  if (result != 3) {
    LOGE(TAG, "Writing spooldata failed");
    return false;
  }

  // encrypt the tag if it wasn't yet
  if (!_encrypted) {
    uint8_t blockData[16];
    // read sector trailer from tag
    if (!nfc->mifareclassic_ReadDataBlock(7, blockData)) {
      LOGE(TAG, "Reading sector trailer failed");
      return false;
    }
    // copy our ekey to keyA
    memcpy(blockData, _eKey.keyByte, 6);
    // copy our ekey to keyB
    memcpy(&blockData[10], _eKey.keyByte, 6);
    // write sector trailer
    if (!nfc->mifareclassic_WriteDataBlock(7, blockData)) {
      LOGE(TAG, "Writing sector trailer failed");
      return false;
    }
  }

  // check the data that was just written
  SpoolData written = _spooldata;
  if (!readSpoolData(nfc)) {
    LOGE(TAG, "Reading tag after writing failed!");
    _spooldata = written;
    return false;
  } else {
    if (_spooldata == written) {
      LOGI(TAG, "Writing spooldata successful");
      return true;
    } else {
      LOGW(TAG, "Tag content doesn't match data to be written!");
      _spooldata = written;
      return false;
    }
  }
}
