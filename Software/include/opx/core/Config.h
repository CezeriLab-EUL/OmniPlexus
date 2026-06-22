//
// Created by dunamis on 28/01/2026.
//

#ifndef SMARTDRIVE_CONFIG_H
#define SMARTDRIVE_CONFIG_H

#if defined(__AVR__) || defined(__riscv)
#define LOGGING_ENABLED 0
#define DEBUG_ENABLED 0
#else
#define LOGGING_ENABLED 1
#define DEBUG_ENABLED 1
#endif

#ifndef MAX_TELEMETRY_SOURCES
#if defined(__AVR__) || defined(__riscv)
#define MAX_TELEMETRY_SOURCES 8
#else
#define MAX_TELEMETRY_SOURCES 16
#endif
#endif

#ifndef MAX_SETTINGS
#if defined(__AVR__) || defined(__riscv)
#define MAX_SETTINGS 8
#else
#define MAX_SETTINGS 16
#endif
#endif

#ifndef COMMAND_QUEUE_CAPACITY
#if defined(__AVR__) || defined(__riscv)
#define COMMAND_QUEUE_CAPACITY 2
#else
#define COMMAND_QUEUE_CAPACITY 8
#endif
#endif

#ifndef PENDING_ACK_CAPACITY
#if defined(__AVR__) || defined(__riscv)
#define PENDING_ACK_CAPACITY 2
#else
#define PENDING_ACK_CAPACITY 8
#endif
#endif

#ifndef MAX_TRANSPORTS
#if defined(__AVR__) || defined(__riscv)
#define MAX_TRANSPORTS 2
#else
#define MAX_TRANSPORTS 4
#endif
#endif

#ifndef MAX_FORWARDING_PAIRS
#if defined(__AVR__) || defined(__riscv)
#define MAX_FORWARDING_PAIRS 2
#else
#define MAX_FORWARDING_PAIRS 4
#endif
#endif

#ifndef MAX_DISCOVERED_DEVICES
#if defined(__AVR__) || defined(__riscv)
#define MAX_DISCOVERED_DEVICES 4
#else
#define MAX_DISCOVERED_DEVICES 4
#endif
#endif

#ifndef CDNC_ENABLED
#define CDNC_ENABLED 1
#endif

#if defined(__riscv) || defined(__AVR__)
#define OPX_SLAVE_MINIMAL 1
#endif

#endif // SMARTDRIVE_CONFIG_H
