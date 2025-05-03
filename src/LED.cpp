// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */

#include <math.h>
#include <thingy.h>

#define TAG "RFID"

#define LEDC_DUTY_RES    (8)
#define LED_BRIGHT_OFF   (0)
#define LED_BRIGHT_DIM   (50)
#define LED_BRIGHT_PULSE (120)
#define LED_BRIGHT_FULL  (255)
#define LEDC_FREQ        (4000)
#define DEFAULT_SAT      240
#define DEFAULT_VALUE    255

// const char* fmtMemCk = "Free: %d MaxAlloc: %d PSFree: %d";
// #define MEMCK LOGD(TAG, fmtMemCk, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram())

void LED::begin(Scheduler* scheduler) {
  // Task handling
  _scheduler = scheduler;

  // create and run a task for setting up the led
  Task* ledInitTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _ledInitCallback(); }, _scheduler, false, NULL, NULL, true);
  ledInitTask->enable();
}

void LED::end() {
  LOGD(TAG, "Stopping...");
  if (_ledTask != nullptr) {
    _ledTask->disable();
  }

  //_ledDisable();
  LOGD(TAG, "...done!");
}

void LED::_ledInitCallback() {
  LOGD(TAG, "Starting LED...");

  // use LEDC PWM timer for plain LEDs
  if (!_isRGB) {
    ledcAttach(_ledPin, LEDC_FREQ, LEDC_DUTY_RES);
    ledcWrite(_ledPin, 0);
  }

  // set current mode to none
  _mode = LEDMode::NONE;
  setMode(LEDMode::WAITING_WIFI);
}

void LED::setMode(LEDMode mode) {
  // Nothing to do
  if (_mode == mode)
    return;

  // disable the current task
  if (_ledTask != nullptr) {
    _ledTask->disable();
    _ledTask = nullptr;
  }

  // set the new mode
  _mode = mode;
  switch (_mode) {
    case LEDMode::WAITING_WIFI: {
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_OFF);
      } else {
        _ledState = 0;
        rgbLedWrite(_ledPin, 0, 0, 0);
      }
      // Set LED to blinking / blinking white
      _ledTask = new Task(400, TASK_FOREVER, [&] {
        if (!_isRGB) {
          if (ledcRead(_ledPin) == LED_BRIGHT_DIM) {
            ledcWrite(_ledPin, LED_BRIGHT_OFF);
          } else {
            ledcWrite(_ledPin, LED_BRIGHT_DIM);
          }
        } else {
          CRGB led_color(CRGB::HTMLColorCode::Black);
          if (!_ledState) {
            led_color = CRGB(CHSV(HUE_BLUE, 0, LED_BRIGHT_DIM));
            _adjustLed(&led_color, _colorAdjustment);
            _ledState = 1;
          } else {
            _ledState = 0;
          }
          rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::WAITING_CAPTIVE: {
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_OFF);
      } else {
        _ledState = 0;
        rgbLedWrite(_ledPin, 0, 0, 0);
      }
      // Set LED to fast blinking / fast blinking white
      _ledTask = new Task(100, TASK_FOREVER, [&] {
        if (!_isRGB) {
          if (ledcRead(_ledPin) == LED_BRIGHT_DIM) {
            ledcWrite(_ledPin, LED_BRIGHT_OFF);
          } else {
            ledcWrite(_ledPin, LED_BRIGHT_DIM);
          }
        } else {
          CRGB led_color(CRGB::HTMLColorCode::Black);
          if (!_ledState) {
            led_color = CRGB(CHSV(HUE_BLUE, 0, LED_BRIGHT_DIM));
            _adjustLed(&led_color, _colorAdjustment);
            _ledState = 1;
          } else {
            _ledState = 0;
          }
          rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::WAITING_READ: {
      // set LED to dim solid / dim solid white
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_DIM);
      } else {
        CRGB led_color(CHSV(HUE_BLUE, DEFAULT_SAT, LED_BRIGHT_DIM));
        _adjustLed(&led_color, _colorAdjustment);
        rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
      }
    } break;
    case LEDMode::TAG_READ: {
      // schedule a task for resetting the brighness to previous level
      Task* resetTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { setMode(LEDMode::WAITING_READ); }, _scheduler, false, NULL, NULL, true);

      // Flash the LED
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_FULL);
      } else {
        CRGB led_color(CHSV(HUE_BLUE, DEFAULT_SAT, LED_BRIGHT_FULL));
        _adjustLed(&led_color, _colorAdjustment);
        rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
      }

      // Start the task to reset to WAITING_READ
      resetTask->enableDelayed(250);
    } break;
    case LEDMode::ARMED_WRITING: {
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_OFF);
      } else {
        _ledState = 0;
        rgbLedWrite(_ledPin, 0, 0, 0);
      }
      // Set LED to breathing / breathing green
      _ledTask = new Task(40, TASK_FOREVER, [&] {
        // breathing from 0 to 100
        uint8_t brightness = (exp(sin(millis() / 1000.0 * PI)) - 0.368) * 42.546;
        if (!_isRGB) {
          ledcWrite(_ledPin, brightness);
        } else {
          CRGB led_color(CHSV(HUE_GREEN, DEFAULT_SAT, brightness));
          _adjustLed(&led_color, _colorAdjustment);
          rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::ARMED_REWRITING: {
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_OFF);
      } else {
        _ledState = 0;
        rgbLedWrite(_ledPin, 0, 0, 0);
      }
      // Set LED to fast breathing / breathing pulsing red
      _ledTask = new Task(40, TASK_FOREVER, [&] {
        // breathing from 0 to 100
        uint8_t brightness = (exp(sin(millis() / 500.0 * PI)) - 0.368) * 42.546;
        if (!_isRGB) {
          ledcWrite(_ledPin, brightness);
        } else {
          CRGB led_color(CHSV(HUE_RED, DEFAULT_SAT, brightness));
          _adjustLed(&led_color, _colorAdjustment);
          rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::TAG_WRITTEN: {
      // schedule a task for resetting the brighness to previous level
      Task* resetTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { setMode(LEDMode::ARMED_WRITING); }, _scheduler, false, NULL, NULL, true);

      // Flash the LED
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_FULL);
      } else {
        CRGB led_color(CHSV(HUE_GREEN, DEFAULT_SAT, LED_BRIGHT_FULL));
        _adjustLed(&led_color, _colorAdjustment);
        rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
      }

      // Start the task to reset to previous state
      resetTask->enableDelayed(250);
    } break;
    case LEDMode::TAG_REWRITTEN: {
      // schedule a task for resetting the brighness to previous level
      Task* resetTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { setMode(LEDMode::ARMED_REWRITING); }, _scheduler, false, NULL, NULL, true);

      // Flash the LED
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_FULL);
      } else {
        CRGB led_color(CHSV(HUE_RED, DEFAULT_SAT, LED_BRIGHT_FULL));
        _adjustLed(&led_color, _colorAdjustment);
        rgbLedWrite(_ledPin, led_color.red, led_color.green, led_color.blue);
      }

      // Start the task to reset to previous state
      resetTask->enableDelayed(250);
    } break;
    default: // switch it off
      if (!_isRGB) {
        ledcWrite(_ledPin, 0);
      } else {
        rgbLedWrite(_ledPin, 0, 0, 0);
      }
  }
}
