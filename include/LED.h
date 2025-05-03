// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <FastLED.h>
#include <TaskSchedulerDeclarations.h>
#include <thingy.h>

#if defined(RGB_BUILTIN) || defined(RGB_EXTERNAL)
  #define IS_RGB true
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

#if defined(RGB_EXTERNAL)
    explicit LED(int ledPin = RGB_EXTERNAL, bool isRGB = IS_RGB) : _ledPin(ledPin), _isRGB(isRGB) {}
#else
    explicit LED(int ledPin = LED_BUILTIN, bool isRGB = IS_RGB) : _ledPin(ledPin), _isRGB(isRGB) {}
#endif
    void begin(Scheduler* scheduler);
    void end();
    void setMode(LEDMode mode);

  private:
    int _ledPin;
    bool _isRGB;
    Scheduler* _scheduler = nullptr;
    Task* _ledTask = nullptr;
    uint8_t _ledState = 0;
    void _ledInitCallback();
    LEDMode _mode = LEDMode::WAITING_WIFI;
#ifdef COLOR_CORR_SCALE
    CRGB _colorAdjustment = CRGB::computeAdjustment(COLOR_CORR_SCALE, CRGB(COLOR_CORR_R, COLOR_CORR_G, COLOR_CORR_B), CRGB(UncorrectedTemperature));
    static void _adjustLed(CRGB* led, const CRGB& adjustment) {
      led->red = scale8(led->red, adjustment.red);
      led->green = scale8(led->green, adjustment.green);
      led->blue = scale8(led->blue, adjustment.blue);
    }
#else
    CRGB _colorAdjustment = CRGB(0xFF, 0xFF, 0xFF);
    static void _adjustLed(CRGB* led, const CRGB& adjustment) {}
#endif
};
