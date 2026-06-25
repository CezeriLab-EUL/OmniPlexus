//
// Created by dunamis on 27/03/2026.
//

#ifndef SMARTDRIVE_IPLATFORMCLOCK_H
#define SMARTDRIVE_IPLATFORMCLOCK_H

#include "opx/shared/core/platform.h" // IWYU pragma: keep

class IPlatformClock {
public:
  virtual uint32_t millis() const = 0;
};

#endif // SMARTDRIVE_IPLATFORMCLOCK_H