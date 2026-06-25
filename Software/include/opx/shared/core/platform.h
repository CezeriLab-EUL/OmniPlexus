//
// Created by dunamis on 23/02/2026.
//

#ifndef SMARTDRIVE_PLATFORM_H
#define SMARTDRIVE_PLATFORM_H

#include "opx/shared/core/Config.h"

#ifdef OPX_FRAMEWORK_ARDUINO
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
using ::size_t;
    #ifdef OPX_TARGET_AVR
    #define OPX_PLATFORM_AVR
    #endif
#else
#include <cstdint>
#include <cstdlib>
#include <cstring>
using std::size_t;
#endif


#endif //SMARTDRIVE_PLATFORM_H