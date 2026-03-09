//
// Created by dunamis on 28/01/2026.
//

#ifndef SMARTDRIVE_CONFIG_H
#define SMARTDRIVE_CONFIG_H

#ifdef __AVR__
    #define LOGGING_ENABLED 0
    #define DEBUG_ENABLED 0
#else
    #define LOGGING_ENABLED 1
    #define DEBUG_ENABLED 1
#endif
#define MAX_NUM_MODULES 8

#ifndef COMMAND_QUEUE_CAPACITY
#ifdef __AVR__
#define COMMAND_QUEUE_CAPACITY 2
#else
#define COMMAND_QUEUE_CAPACITY 8
#endif
#endif

#ifndef PENDING_ACK_CAPACITY
#ifdef __AVR__
#define PENDING_ACK_CAPACITY 2
#else
#define PENDING_ACK_CAPACITY 8
#endif
#endif


#endif //SMARTDRIVE_CONFIG_H