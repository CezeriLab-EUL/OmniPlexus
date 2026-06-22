//
// Created by dunamis on 27/03/2026.
//

#ifndef SMARTDRIVE_PLATFORMCLOCK_H
#define SMARTDRIVE_PLATFORMCLOCK_H

#include "opx/interfaces/IPlatformClock.h"

#ifdef ARDUINO
#include <Arduino.h>

class PlatformClock : public IPlatformClock {
public:
    uint32_t millis() const override {
        return ::millis();
    }
};
#elif defined(STM32)
#include "stm32_hal.h"

class PlatformClock : public IPlatformClock {
public:
    uint32_t millis() const override {
        return HAL_GetTick();
    }
};
#else
#include <chrono>

class PlatformClock : public IPlatformClock {
public:
    uint32_t millis() const override {
        using namespace std::chrono;
        return static_cast<uint32_t>(
            duration_cast<milliseconds>(
                steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

#endif

#endif //SMARTDRIVE_PLATFORMCLOCK_H