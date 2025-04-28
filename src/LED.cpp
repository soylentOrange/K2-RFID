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

// const char* fmtMemCk = "Free: %d MaxAlloc: %d PSFree: %d";
// #define MEMCK LOGD(TAG, fmtMemCk, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram())
//RGB_BUILTIN_LED_COLOR_ORDER
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
  } else {
    LOGE(TAG, "Not implemented");
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
      ledcWrite(_ledPin, LED_BRIGHT_OFF);
      // Set LED to blinking / blinking blue
      _ledTask = new Task(400, TASK_FOREVER, [&] {
        if (!_isRGB) {
          if (ledcRead(_ledPin) == LED_BRIGHT_DIM) {
            ledcWrite(_ledPin, LED_BRIGHT_OFF);
          } else {
            ledcWrite(_ledPin, LED_BRIGHT_DIM);
          }
        } else {
            LOGE(TAG, "Not implemented");
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::WAITING_CAPTIVE: {
      ledcWrite(_ledPin, LED_BRIGHT_OFF);
      // Set LED to fast blinking / fast blinking blue
      _ledTask = new Task(100, TASK_FOREVER, [&] {
        if (!_isRGB) {
          if (ledcRead(_ledPin) == LED_BRIGHT_DIM) {
            ledcWrite(_ledPin, LED_BRIGHT_OFF);
          } else {
            ledcWrite(_ledPin, LED_BRIGHT_DIM);
          }
        } else {
            LOGE(TAG, "Not implemented");
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::WAITING_READ: {
      // set LED to dim solid / dim solid white
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_DIM);
      } else {
        LOGE(TAG, "Not implemented");
      }
    } break;
    case LEDMode::TAG_READ: {
      // schedule a task for resetting the brighness to previous level
      Task* resetTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { setMode(LEDMode::WAITING_READ); }, _scheduler, false, NULL, NULL, true);

      // Flash the LED
      if (!_isRGB) {
        ledcWrite(_ledPin, LED_BRIGHT_FULL);
      } else {
        LOGE(TAG, "Not implemented");
      }

      // Start the task to reset to WAITING_READ
      resetTask->enableDelayed(250);
    } break;
    case LEDMode::ARMED_WRITING: {
      ledcWrite(_ledPin, LED_BRIGHT_OFF);
      // Set LED to breathing / breathing orange
      _ledTask = new Task(40, TASK_FOREVER, [&] {
        if (!_isRGB) {
          // breathing from 0 to 100
          uint8_t brightness = (exp(sin(millis() / 1000.0 * PI)) - 0.368) * 42.546;
          ledcWrite(_ledPin, brightness);
        } else {
          LOGE(TAG, "Not implemented");
        } }, _scheduler, false, NULL, NULL, true);
      _ledTask->enable();
    } break;
    case LEDMode::ARMED_REWRITING: {
      ledcWrite(_ledPin, LED_BRIGHT_OFF);
      // Set LED to fast breathing / breathing pulsing red
      _ledTask = new Task(40, TASK_FOREVER, [&] {
        if (!_isRGB) {
          // breathing from 0 to 100
          uint8_t brightness = (exp(sin(millis() / 500.0 * PI)) - 0.368) * 42.546;
          ledcWrite(_ledPin, brightness);
        } else {
          LOGE(TAG, "Not implemented");
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
        LOGE(TAG, "Not implemented");
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
        LOGE(TAG, "Not implemented");
      }

      // Start the task to reset to previous state
      resetTask->enableDelayed(250);
    } break;
    default: // switch it off
      ledcWrite(_ledPin, 0);
  }
}
