//
// Created by dunamis on 30/04/2026.
//

#ifndef SMARTDRIVE_FREERTOSMUTEX_H
#define SMARTDRIVE_FREERTOSMUTEX_H

#include "opx/shared/core/Config.h" // IWYU pragma: keep

#if OPX_HAS_FREERTOS
#include "opx/shared/interfaces/IMutex.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class FreeRtosMutex : public IMutex {
  SemaphoreHandle_t mutex;

public:
  FreeRtosMutex() { mutex = xSemaphoreCreateMutex(); }
  ~FreeRtosMutex() {
    if (mutex) {
      vSemaphoreDelete(mutex);
    }
  }
  void lock() override { xSemaphoreTake(mutex, portMAX_DELAY); }
  void unlock() override { xSemaphoreGive(mutex); }
};
#endif

#endif // SMARTDRIVE_FREERTOSMUTEX_H