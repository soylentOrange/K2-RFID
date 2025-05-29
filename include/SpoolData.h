// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <ArduinoJson.h>

#include <algorithm>
#include <iostream>
#include <string>
using std::runtime_error;

struct SpoolData {
  public:
    // empty constructor
    SpoolData() = default;

    // copy constructor
    SpoolData(const SpoolData& rhs) {
      _spooldata = rhs._spooldata;
      _materialBatch = rhs._materialBatch;
      _materialDate = rhs._materialDate;
      _materialVendor = rhs._materialVendor;
      _materialType = rhs._materialType;
      _materialWeight = rhs._materialWeight;
      _materialColorNumeric = rhs._materialColorNumeric;
      _materialColorString = rhs._materialColorString;
      _serialNum = rhs._serialNum;
      _reserve = rhs._reserve;
    }

    // assignment operator
    SpoolData& operator=(const SpoolData& rhs) {
      if (this != &rhs) { // protect against invalid self-assignment
        _spooldata = rhs._spooldata;
        _materialBatch = rhs._materialBatch;
        _materialDate = rhs._materialDate;
        _materialVendor = rhs._materialVendor;
        _materialType = rhs._materialType;
        _materialWeight = rhs._materialWeight;
        _materialColorNumeric = rhs._materialColorNumeric;
        _materialColorString = rhs._materialColorString;
        _serialNum = rhs._serialNum;
        _reserve = rhs._reserve;
      }
      return *this;
    }

    // Equality operator
    bool operator==(const SpoolData& rhs) {
      return _spooldata == rhs._spooldata;
    }

    // Constructor from JSON
    explicit SpoolData(const JsonDocument& spooldata) {
      std::string colorString(spooldata["color"].as<const char*>());
      _materialColorNumeric = std::stoi(colorString.substr(1).c_str(), nullptr, 16);
      _materialColorString = _convertMaterialColorString(_materialColorNumeric, '#');
      _materialType = spooldata["type"].as<const char*>();

      // use provided weight
      _materialWeight = spooldata["weight"].as<const uint32_t>();

      // use default Vendor (Creality)
      _materialVendor = _materialVendor_Creality;

      // use provided batch or default
      if (spooldata["batch"].is<const char*>())
        _materialBatch = spooldata["batch"].as<const char*>();
      else
        _materialBatch = _materialBatch_default;

      // use provided date or default
      if (spooldata["date"].is<const char*>())
        _materialDate = spooldata["date"].as<const char*>();
      else
        _materialDate = _materialDate_default;

      // use provided reserve or default
      if (spooldata["reserve"].is<const char*>())
        _reserve = spooldata["reserve"].as<const char*>();
      else
        _reserve = "000000";

      // make up a serial number if not available
      if (spooldata["serial"].is<const char*>())
        _serialNum = spooldata["serial"].as<const char*>();
      else
        _serialNum = std::to_string(random(100000, 999999));

      // generate spooldata string
      _generateSpooldataString();
    }

    // Constructor from spooldata string
    // beware: will throw exception in case of failure
    explicit SpoolData(const std::string& spooldata) {
      // get production date
      _materialDate = spooldata.substr(0, 5);

      // get vendor
      _materialVendor = spooldata.substr(5, 4);

      // get batch
      _materialVendor = spooldata.substr(9, 2);

      // get material type (ignore the leading '1')
      _materialType = spooldata.substr(12, 5);

      // get material color (ignore the leading '0')
      _materialColorNumeric = std::stoi(spooldata.substr(18, 6), nullptr, 16);
      _materialColorString = _convertMaterialColorString(_materialColorNumeric, '#');

      // calculate material weight from material length (in m)
      _materialWeight = _convertMaterialWeight(std::stoi(spooldata.substr(24, 4)));

      // get serial number
      _serialNum = spooldata.substr(28, 6);

      // get the remainder (beware strange content might be in there...)
      _reserve = spooldata.substr(34);

      // copy spooldata string
      _spooldata = spooldata;
    }

    // typecast to JSON Document
    explicit operator JsonDocument() const {
      JsonDocument spooldata;
      spooldata["spooldata"] = _spooldata;
      spooldata["batch"] = _materialBatch;
      spooldata["date"] = _materialDate;
      spooldata["vendor"] = _materialVendor;
      spooldata["type"] = _materialType;
      spooldata["weight"] = _materialWeight;
      spooldata["color"] = _materialColorString;
      spooldata["serial"] = _serialNum;
      spooldata["reserve"] = _reserve;
      spooldata.shrinkToFit();
      return spooldata;
    }

    // typecast to std::string
    explicit operator std::string() const {
      return _spooldata;
    }

    friend class CFSTag;

  private:
    std::string _spooldata = "";
    std::string _materialBatch = "";
    std::string _materialDate = "";
    std::string _materialVendor = "";
    std::string _materialType = "";
    uint32_t _materialWeight = 0;
    uint32_t _materialColorNumeric = 0;
    std::string _materialColorString = "";
    std::string _serialNum = "";
    std::string _reserve = "";
    static constexpr char* _materialVendor_Creality = "0276";
    static constexpr char* _materialBatch_default = "A2";
    static constexpr char* _materialDate_default = "AB124";

    // weight (in g) to length (in m)
    uint32_t inline _convertMaterialLength(const uint32_t& weight) {
      return 330 * weight / 1000;
    }

    // weight (in g) to length (in m, left zero padded)
    std::string inline _convertMaterialLengthString(const uint32_t& weight) {
      std::string lengthString = std::to_string(_convertMaterialLength(weight));
      lengthString.insert(lengthString.begin(), 4 - lengthString.size(), '0');
      return lengthString;
    }

    // (numeric) color to color string
    std::string inline _convertMaterialColorString(const uint32_t& color, char pad = 0) {
      char col_str[8];
      if (pad != 0) {
        snprintf(col_str, sizeof(col_str), "%c%06X", pad, color);
      } else {
        snprintf(col_str, sizeof(col_str), "%06X", color);
      }

      return std::string(col_str);
    }

    // length (in m) to weight (in g)
    uint32_t inline _convertMaterialWeight(const uint32_t& length) {
      return 1000 * length / 330;
    }

    // generate spooldata string from privates
    void _generateSpooldataString() {
      _spooldata = "";
      _spooldata += _materialDate;
      _spooldata += _materialVendor;
      _spooldata += _materialBatch;
      _spooldata += "1";
      _spooldata += _materialType;
      _spooldata += _convertMaterialColorString(_materialColorNumeric, '0');
      _spooldata += _convertMaterialLengthString(_materialWeight);
      _spooldata += _serialNum;

      // make it uppercase (just in case)
      std::transform(_spooldata.begin(), _spooldata.end(), _spooldata.begin(), [](auto c) { return std::toupper(c); });
    }
};
