//
// Created by dunamis on 27/03/2026.
//

#pragma once

#include "opx/shared/core/platform.h" // IWYU pragma: keep

class IPlatformClock {
public:
  virtual uint32_t millis() const = 0;
};
