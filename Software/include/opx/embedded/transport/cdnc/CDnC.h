// CDnC.h
// CDnC half-duplex parallel transport — master side hardware header.
// Physical layer implemented in CDnC.cpp for STM32F4xx (GPIOE) and
// STM32F1xx Blue Pill (GPIOB).
//
// Slave builds include CDnCConfig.h directly — NOT this file.

#pragma once
#include "CDnCConfig.h"

// CDnC.cpp is compiled under #if defined(STM32F4xx) || defined(STM32F1xx)
