//
// Created by dunamis on 23/04/2026.
//

#ifndef SMARTDRIVE_ESPHTTPTRANSPORT_H
#define SMARTDRIVE_ESPHTTPTRANSPORT_H

#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>
#include "smartdrive/interfaces/ITransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"
#include "types.h"

// 4096 bytes is comfortable for httplib's internal parsing + your lambda.
// If you add heavy processing inside the handler, increase this.
static constexpr uint32_t HTTP_TASK_STACK_SIZE  = 4096;
static constexpr UBaseType_t HTTP_TASK_PRIORITY = 1;
static constexpr BaseType_t  HTTP_TASK_CORE     = 0;  // pin to core 0, leave core 1 for the main loop
static constexpr const char* HTTP_ENDPOINT = "/smartdrive";

class EspHttpTransport;
static void httpServerTask(void* param);

class EspHttpTransport : public ITransport {

};

#endif //SMARTDRIVE_ESPHTTPTRANSPORT_H