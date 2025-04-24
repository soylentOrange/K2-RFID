// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <Adafruit_PN532.h>
#include <SpoolData.h>

#include <algorithm>
#include <string>

#include "mbedtls/aes.h"

struct CFSTag {
  public:
    // A struct used for passing a MIFARE Crypto1 key
    typedef struct {
        uint8_t keyByte[6];
    } MIFARE_Key;

    // A struct used for passing an AES128 key
    typedef struct {
        uint8_t keyByte[16];
    } AES128_Key;

    // A struct used for passing three blocks of MIFARE data
    typedef union {
        uint8_t blockData[3][16];
        char data[48];
    } MIFARE_tripleBlock;

    // A struct used for passing the UID
    typedef struct Uid {
        uint8_t size; // Number of bytes in the UID. Only 4 (MIFARE classic) is valid
        uint8_t uidByte[10];

        explicit operator std::string() const {
          char uid_str[9];
          snprintf(uid_str, sizeof(uid_str), "%02x%02x%02x%02x", uidByte[0], uidByte[1], uidByte[2], uidByte[3]);
          std::string tagUidStr(uid_str);
          return tagUidStr;
        }

        // empty default constructor
        Uid() : size(4) { memset(uidByte, 0, sizeof(uidByte)); }

        // constructor from data (only size of ≤ 10 is considered)
        Uid(uint8_t size, uint8_t* data) : size(size) {
          memcpy(uidByte, data, min<uint8_t>(size, 10));
        }

        // copy constructor
        Uid(const Uid& uid) {
          size = uid.size;
          memcpy(uidByte, uid.uidByte, uid.size);
        }
    } Uid;

    // empty constructor
    CFSTag() {
      _uid = Uid();
      _spooldata = SpoolData();
      _eKey = createKey(_uid);
    }

    // constructor from Uid
    explicit CFSTag(const Uid& uid_in) {
      _uid = Uid(uid_in);
      _spooldata = SpoolData();
      _eKey = createKey(_uid);
    }

    // copy constructor
    CFSTag(const CFSTag& rhs) {
      _uid = Uid(rhs._uid);
      _encrypted = rhs._encrypted;
      _eKey = rhs._eKey;
      _spooldata = rhs._spooldata;
      _validMaterial = rhs._validMaterial;
      _empty = rhs._empty;
    }

    // assignment operator
    CFSTag& operator=(const CFSTag& rhs) {
      if (this != &rhs) { // protect against invalid self-assignment
        _uid = Uid(rhs._uid);
        _encrypted = rhs._encrypted;
        _eKey = rhs._eKey;
        _spooldata = rhs._spooldata;
        _validMaterial = rhs._validMaterial;
        _empty = rhs._empty;
      }
      return *this;
    }

    // constructor from PN532 nfc reader
    // beware: will throw exception in case of failure
    explicit CFSTag(Adafruit_PN532* nfc);

    // equality operator (only considers uid!)
    bool operator==(const CFSTag& rhs) const {
      return static_cast<std::string>(rhs._uid) == static_cast<std::string>(_uid);
    }

    // inequality operator
    bool operator!=(const CFSTag& rhs) const {
      return !operator==(rhs);
    }

    // read spooldata from tag
    // returns true on success (still, the spool could be empty)
    bool readSpoolData(Adafruit_PN532* nfc);

    // write spooldata to tag
    // returns true on success
    bool writeSpoolData(Adafruit_PN532* nfc, SpoolData spooldata);

    // get the uid
    Uid getUid() {
      return _uid;
    }

    // get spooldata
    SpoolData getSpooldata() {
      return _spooldata;
    }

    // is the tag yet unwritten
    bool isEmpty() {
      return _empty;
    }

    // Default key
    static constexpr MIFARE_Key std_key = {{255, 255, 255, 255, 255, 255}};
    static constexpr AES128_Key u_key = {{113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49}};
    static constexpr AES128_Key d_key = {{72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50}};
    friend class RFID;

  private:
    Uid _uid;
    MIFARE_Key _eKey = MIFARE_Key();
    bool _encrypted = false;
    bool _validMaterial = false;
    bool _empty = true;
    SpoolData _spooldata = SpoolData();

    // decrypt a triple block of MIFARE data
    // returns true on success
    static bool decrypt(const MIFARE_tripleBlock& input, const MIFARE_tripleBlock& output) {
      int result = 0;
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_enc(&aes, d_key.keyByte, 128);
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input.blockData[0], const_cast<uint8_t*>(output.blockData[0]));
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input.blockData[1], const_cast<uint8_t*>(output.blockData[1]));
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input.blockData[2], const_cast<uint8_t*>(output.blockData[2]));
      mbedtls_aes_free(&aes);

      if (result)
        return false;
      else
        return true;
    }

    // encrypt a triple block of MIFARE data
    // returns true on success
    static bool encrypt(const MIFARE_tripleBlock& input, const MIFARE_tripleBlock& output) {
      int result = 0;
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_enc(&aes, d_key.keyByte, 128);
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input.blockData[0], const_cast<uint8_t*>(output.blockData[0]));
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input.blockData[1], const_cast<uint8_t*>(output.blockData[1]));
      result += mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input.blockData[2], const_cast<uint8_t*>(output.blockData[2]));
      mbedtls_aes_free(&aes);

      if (result)
        return false;
      else
        return true;
    }

    // create MIFARE key based on UID of tag
    static MIFARE_Key createKey(const Uid& uid) {
      MIFARE_Key eKey = MIFARE_Key();
      uint8_t inputBuf[16];
      uint8_t outputBuf[16];
      for (size_t i = 0; i < 4; ++i) {
        memcpy(inputBuf + i * 4, uid.uidByte, 4);
      }

      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_enc(&aes, u_key.keyByte, 128);
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, inputBuf, outputBuf);
      mbedtls_aes_free(&aes);
      memcpy(eKey.keyByte, outputBuf, 6);

      return eKey;
    }
};
