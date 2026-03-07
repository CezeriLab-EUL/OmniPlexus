//
// Created by dunamis on 29/01/2026.
//

#ifndef SMARTDRIVE_IENCODER_H
#define SMARTDRIVE_IENCODER_H

#include "../types/ProtocolTypes.h"
#include "../types/RobotData.h"

class IEncoder {
public:
    virtual ~IEncoder() = default;

    //Command Serialization & Deserialization
    virtual SerializedData serializeCommand(const Command& cmd) = 0;
    virtual bool deserializeCommand(const RawData& rawData, Command& cmdOut) = 0;
    virtual bool extractCommandPayload(const RawData& frame, uint8_t* payloadOut, uint8_t& payloadSizeOut) = 0;

    //Discovery Serialization & Deserialization
    virtual SerializedData serializeDiscovery(const DiscoveryResponse& resp) = 0;
    virtual bool deserializeDiscovery(const RawData& rawData, DiscoveryResponse& respOut) = 0;

    //ValueSource Serialization & Deserialization
    virtual SerializedData serializeValue(const ValueSource& value) = 0;
    virtual bool deserializeValue(const RawData& rawData, ValueSource& valueOut) = 0;

    //Telemetry Serialization & Deserialization
    virtual SerializedData serializeTelemetry(const TelemetryData& telemetry) = 0;
    virtual bool deserializeTelemetry(const RawData& rawData, TelemetryData& telemetryOut) = 0;

    //Settings Serialization & Deserialization
    virtual SerializedData serializeSettings(const SettingsData& settings) = 0;
    virtual bool deserializeSettings(const RawData& rawData, SettingsData& settingsOut) = 0;

    virtual uint8_t computeIntegrityCode(const RawData& rawData) = 0;
};

#endif //SMARTDRIVE_IENCODER_H