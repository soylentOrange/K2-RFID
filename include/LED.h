// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <thingy.h>

#ifdef RGB_BUILTIN
  #define IS_RGB true
  #include <FastLED.h>
#else
  #define IS_RGB false
#endif

class LED {
  public:
    enum class LEDMode {
      NONE,
      WAITING_WIFI,
      WAITING_CAPTIVE,
      WAITING_READ,
      TAG_READ,
      ARMED_WRITING,
      ARMED_REWRITING,
      TAG_WRITTEN,
      TAG_REWRITTEN
    };

    explicit LED(int ledPin = LED_BUILTIN, bool isRGB = IS_RGB) : _ledPin(ledPin), _isRGB(isRGB) {}
    void begin(Scheduler* scheduler);
    void end();
    void setMode(LEDMode mode);

  private:
    int _ledPin;
    bool _isRGB;
    Scheduler* _scheduler = nullptr;
    Task* _ledTask = nullptr;
    void _ledInitCallback();
    LEDMode _mode = LEDMode::WAITING_WIFI;
};
