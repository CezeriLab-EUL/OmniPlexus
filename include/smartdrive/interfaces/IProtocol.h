//
// Created by dunamis on 29/01/2026.
//

#ifndef SMARTDRIVE_IPROTOCOL_H
#define SMARTDRIVE_IPROTOCOL_H

#include <cstdint>
#include "../constants/ProtocolConstants.h"
#include "../types/ProtocolTypes.h"
#include "../types/RobotData.h"

struct SerializedData {
    uint8_t data[ProtocolConstants::MAX_FRAME_SIZE];
    size_t size = 0;
};

class IProtocol {
public:
    virtual ~IProtocol() = default;

    //Command Serialization & Deserialization
    virtual SerializedData serializeCommand(const Command& cmd) = 0;
    virtual bool deserializeCommand(const uint8_t* data, size_t size, Command& cmdOut) = 0;

    //Discovery Serialization & Deserialization
    virtual SerializedData serializeDiscovery(const DiscoveryResponse& resp) = 0;
    virtual bool deserializeDiscovery(const uint8_t* data, size_t size, DiscoveryResponse& respOut) = 0;

    //ValueSource Serialization & Deserialization
    virtual SerializedData serializeValue(const ValueSource& value) = 0;
    virtual bool deserializeValue(const uint8_t* data, size_t size, ValueSource& valueOut) = 0;

    //Telemetry Serialization & Deserialization
    virtual SerializedData serializeTelemetry(const TelemetryData& telemetry) = 0;
    virtual bool deserializeTelemetry(const uint8_t* data, size_t size, TelemetryData& telemetryOut) = 0;

    //Settings Serialization & Deserialization
    virtual SerializedData serializeSettings(const SettingsData& settings) = 0;
    virtual bool deserializeSettings(const uint8_t* data, size_t size, SettingsData& settingsOut) = 0;

    virtual uint16_t computeIntegrityCode(const uint8_t* data, size_t length) = 0;
};

#endif //SMARTDRIVE_IPROTOCOL_H