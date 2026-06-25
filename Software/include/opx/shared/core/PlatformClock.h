//
// Created by dunamis on 27/03/2026.
//

#pragma once

#include "opx/shared/core/Config.h" // IWYU pragma: keep
#include "opx/shared/interfaces/IPlatformClock.h"

#ifdef OPX_FRAMEWORK_ARDUINO
#include <Arduino.h>

class PlatformClock : public IPlatformClock {
public:
  uint32_t millis() const override { return ::millis(); }
};
#elif OPX_TARGET_STM32
#include "stm32_hal.h"

class PlatformClock : public IPlatformClock {
public:
  uint32_t millis() const override { return HAL_GetTick(); }
};
#else
#include <chrono>

class PlatformClock : public IPlatformClock {
public:
  uint32_t millis() const override {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count());
  }
};

#endif
