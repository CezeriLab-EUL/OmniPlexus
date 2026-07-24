#pragma once

#ifdef OPX_TARGET_EMBEDDED
#include <Arduino.h>

#include "opx/shared/core/CommunicationManager.h"
#include "opx/shared/core/Config.h"
#include "opx/shared/core/DeviceRegistry.h"
#include "opx/shared/core/PlatformClock.h"
#include "opx/shared/core/SettingsManager.h"
#include "opx/shared/core/TelemetryManager.h"
#include "opx/shared/core/TransportManager.h"
#include "opx/shared/core/TriggerConfig.h"
#include "opx/shared/core/ValueSource.h"
#include "opx/shared/core/platform.h"
#include "opx/shared/core/string.hpp"

#include "opx/shared/interfaces/IConnectable.h"
#include "opx/shared/interfaces/IEncoder.h"
#include "opx/shared/interfaces/IMutex.h"
#include "opx/shared/interfaces/IPlatformClock.h"
#include "opx/shared/interfaces/ITransport.h"

#include "opx/shared/protocol/BinaryEncoder.h"

#include "opx/shared/transport/AbstractTransport.h"

#include "opx/shared/constants/ProtocolConstants.h"
#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/types/RobotData.h"

#include "opx/shared/mutex/FreeRtosMutex.h"
#include "opx/shared/mutex/NullMutex.h"

#include "opx/shared/utils/CRC8.h"
#include "opx/shared/utils/CommandQueue.h"
#include "opx/shared/utils/Logger.h"
#include "opx/shared/utils/PendingAckQueue.h"
#include "opx/shared/utils/ResponseQueue.h"

#include "opx/embedded/core/OpxDevice.h"

#include "opx/embedded/transport/serial/ArduinoSerialTransport.h"

#if OPX_TARGET_ESP32
#include "opx/embedded/transport/http/EspHttpTransport.h"
#include "opx/embedded/transport/wifi/EspWiFiTransport.h"
#endif

#if OPX_HAS_CDNC
#if OPX_CDNC_MASTER
#include "opx/embedded/transport/cdnc/CDnC.h"
#include "opx/embedded/transport/cdnc/CDnCManager.h"
#include "opx/embedded/transport/cdnc/CDnCTransport.h"
#endif
#if OPX_CDNC_SLAVE
#include "opx/embedded/transport/cdnc/CDnCSlaveManager.h"
#include "opx/embedded/transport/cdnc/CDnCSlaveTransport.h"
#endif
#endif

#include "autogen/shared/CommandPacker.h"
#include "autogen/shared/CommandTypes.h"
#include "autogen/shared/GeneratedConfig.h"

#include "autogen/embedded/SettingIDs.h"
#include "autogen/embedded/TelemetrySourceIDs.h"

#include "OpxDevices.h"

class ArduinoLogger {
public:
  static void begin(long baudRate = 115200) {
    Serial.begin(baudRate);
    Logger::setCallback([](LogLevel level, const char *message) {
      switch (level) {
      case LogLevel::OP_ERROR:
        Serial.print("[ERROR] ");
        break;
      case LogLevel::OP_WARNING:
        Serial.print("[WARN]  ");
        break;
      case LogLevel::OP_INFO:
        Serial.print("[INFO]  ");
        break;
      case LogLevel::OP_DEBUG:
        Serial.print("[DEBUG] ");
        break;
      }
      Serial.println(message);
    });
  }
};

#endif
