//
// Created by dunamis on 29/01/2026.
//

#pragma once

#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/types/RobotData.h"

class IEncoder {
public:
  virtual ~IEncoder() = default;

  // Command Serialization & Deserialization
  virtual SerializedData serializeCommand(const Command &cmd,
                                          uint8_t seqNum) = 0;
  virtual bool deserializeCommand(const RawData &rawData, Command &cmdOut) = 0;
  virtual bool extractCommandPayload(const RawData &frame, uint8_t *payloadOut,
                                     uint8_t &payloadSizeOut,
                                     uint8_t &seqNUm) = 0;

  // CommandResponse Serialization & Deserialization
  virtual SerializedData serializeResponse(const CommandResponse &response) = 0;
  virtual bool deserializeResponse(const RawData &rawData,
                                   CommandResponse &responseOut) = 0;

  // Telemetry Serialization & Deserialization
  virtual SerializedData serializeTelemetry(const Telemetry &telemetry) = 0;
  virtual bool deserializeTelemetry(const RawData &rawData,
                                    Telemetry &telemetryOut) = 0;

  // Setting Serialization & Deserialization
  virtual SerializedData serializeSetting(const SettingsData &setting) = 0;
  virtual bool deserializeSetting(const RawData &rawData,
                                  SettingsData &settingOut) = 0;

  virtual uint8_t computeIntegrityCode(const RawData &rawData) = 0;
};
