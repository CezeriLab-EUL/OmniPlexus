#pragma once

// Auto-detected from compiler defines. Do not set manually.

#if defined(ARDUINO)
#if defined(ESP32)
#define OPX_TARGET_ESP32 1
#define OPX_HAS_FREERTOS 1
#define OPX_HAS_WIFI 1
#define OPX_HAS_HTTP 1
#define OPX_HAS_SERIAL 1
#elif defined(ESP8266)
#define OPX_TARGET_ESP8266 1
#define OPX_HAS_FREERTOS 1
#define OPX_HAS_WIFI 1
#define OPX_HAS_SERIAL 1
#elif defined(__AVR__)
#define OPX_TARGET_AVR 1
#define OPX_HAS_SERIAL 1
#define OPX_CONSTRAINED 1
#elif defined(__riscv)
#define OPX_TARGET_CH32 1
#define OPX_HAS_SERIAL 1
#define OPX_CONSTRAINED 1
#elif defined(STM32)
#define OPX_TARGET_STM32 1
#ifdef STM32F4xx
#define OPX_TARGET_STM32F4xx 1
#endif
#ifdef STM32F1xx
#define OPX_TARGET_STM32F1xx 1
#endif
#define OPX_HAS_SERIAL 1
#else
#define OPX_TARGET_ARDUINO_GENERIC 1
#define OPX_HAS_SERIAL 1
#endif
#define OPX_FRAMEWORK_ARDUINO 1
#define OPX_TARGET_EMBEDDED 1

#elif defined(STM32)
// Bare-metal STM32 (no Arduino framework)
#define OPX_TARGET_STM32 1
#ifdef STM32F4xx
#define OPX_TARGET_STM32F4xx 1
#endif
#ifdef STM32F1xx
#define OPX_TARGET_STM32F1xx 1
#endif
#define OPX_HAS_SERIAL 1
#define OPX_TARGET_EMBEDDED 1

#else
// PC / host build
#define OPX_TARGET_PC 1
#define OPX_HAS_WIFI 1
#define OPX_HAS_HTTP 1
#define OPX_HAS_SERIAL 1
#endif

// Define these before including this file (e.g. in your sketch or CMake flags).

#ifndef OPX_HAS_CDNC
#define OPX_HAS_CDNC 0
#endif

#ifndef OPX_CDNC_MASTER
#define OPX_CDNC_MASTER 0
#endif

#ifndef OPX_CDNC_SLAVE
#define OPX_CDNC_SLAVE 0
#endif

// Constrained targets get smaller defaults. Override before including this
// file.

#ifndef MAX_TELEMETRY_SOURCES
#ifdef OPX_CONSTRAINED
#define MAX_TELEMETRY_SOURCES 8
#else
#define MAX_TELEMETRY_SOURCES 16
#endif
#endif

#ifndef MAX_SETTINGS
#ifdef OPX_CONSTRAINED
#define MAX_SETTINGS 8
#else
#define MAX_SETTINGS 16
#endif
#endif

#ifndef COMMAND_QUEUE_CAPACITY
#ifdef OPX_CONSTRAINED
#define COMMAND_QUEUE_CAPACITY 2
#else
#define COMMAND_QUEUE_CAPACITY 8
#endif
#endif

#ifndef PENDING_ACK_CAPACITY
#ifdef OPX_CONSTRAINED
#define PENDING_ACK_CAPACITY 2
#else
#define PENDING_ACK_CAPACITY 8
#endif
#endif

#ifndef MAX_TRANSPORTS
#ifdef OPX_CONSTRAINED
#define MAX_TRANSPORTS 2
#else
#define MAX_TRANSPORTS 4
#endif
#endif

#ifndef MAX_FORWARDING_PAIRS
#ifdef OPX_CONSTRAINED
#define MAX_FORWARDING_PAIRS 2
#else
#define MAX_FORWARDING_PAIRS 4
#endif
#endif

#ifndef MAX_DISCOVERED_DEVICES
#define MAX_DISCOVERED_DEVICES 4
#endif

// Logging

#ifndef LOGGING_ENABLED
#ifdef OPX_CONSTRAINED
#define LOGGING_ENABLED 0
#define DEBUG_ENABLED 0
#else
#define LOGGING_ENABLED 1
#define DEBUG_ENABLED 1
#endif
#endif

// Legacy compatibility
#ifdef OPX_TARGET_EMBEDDED
#define EMBEDDED_BUILD 1
#endif

#if OPX_CDNC_MASTER
#define CDNC_MASTER 1
#endif

#if OPX_CDNC_SLAVE
#define CDNC_SLAVE 1
#endif

#ifdef OPX_CONSTRAINED
#define OPX_SLAVE_MINIMAL 1
#endif
