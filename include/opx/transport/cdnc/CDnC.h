#pragma once
#include "CDnCConfig.h"

//  Master-only hardware implementation 
// CDnC.cpp contains GPIOE bit-plane logic; only meaningful on F4.
// The slave includes CDnCConfig.h directly — NOT CDnC.h.
#ifdef STM32F4xx
// CDnC.cpp is only compiled into the master build via its own guard
#endif