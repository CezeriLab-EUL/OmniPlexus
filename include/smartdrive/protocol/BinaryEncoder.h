//
// Created by dunamis on 28/01/2026.
//

#ifndef SMARTDRIVE_BINARYENCODER_H
#define SMARTDRIVE_BINARYENCODER_H

#include "../interfaces/IEncoder.h"
#include "../types/ProtocolTypes.h"
#include "../types/RobotData.h"
#include "../utils/Logger.h"
#include "../core/Config.h"
#include "../generated/CommandPacker.h"
#include "smartdrive/utils/CRC8.h"

class BinaryEncoder : public IEncoder {
private:
    struct FrameParseResult {
        bool valid;
        size_t payloadStart;
        uint8_t payloadLength;
    };

    FrameParseResult validateFrameHeader(const RawData &rawData,
                                         const ProtocolConstants::FrameType expectedType) const {
        FrameParseResult result{false, 0, 0};

        if (rawData.size < ProtocolConstants::PROTOCOL_OVERHEAD) {
            LOG(LogLevel::ERROR, "Frame too small");
            return result;
        }

        size_t offset = 0;

        const uint8_t header = rawData.data[offset++];
        if (!ProtocolConstants::isValidHeader(header)) {
            LOG(LogLevel::ERROR, "Invalid header");
            return result;
        }

        if (const ProtocolConstants::FrameType frameType = ProtocolConstants::decodeType(header);
            frameType != expectedType) {
            LOG(LogLevel::ERROR, "Frame type mismatch");
            return result;
        }

        result.payloadLength = rawData.data[offset++];
        result.payloadStart = offset;
        result.valid = true;

        return result;
    };

    bool verifyCRC(const RawData &rawData, size_t crcOffset) const {
        const uint8_t receivedCrc = rawData.data[crcOffset];

        const RawData crcData = {rawData.data, crcOffset};

        if (const uint8_t calculatedCRC = CRC8::compute(crcData);
            receivedCrc != calculatedCRC) {
            LOG(LogLevel::ERROR, "CRC mismatch");
            return false;
            }

        return true;
    }

    static inline void writeUint16LE(uint8_t *dest, const uint16_t value) {
        dest[0] = value & 0xFF;
        dest[1] = (value >> 8) & 0xFF;
    }

    static inline uint16_t readUint16LE(const uint8_t *src) {
        return static_cast<uint16_t>(src[0]) |
               (static_cast<uint16_t>(src[1]) << 8);
    }

    static inline void writeUint32LE(uint8_t *dest, const uint32_t value) {
        dest[0] = value & 0xFF;
        dest[1] = (value >> 8) & 0xFF;
        dest[2] = (value >> 16) & 0xFF;
        dest[3] = (value >> 24) & 0xFF;
    }

    static inline uint32_t readUint32LE(const uint8_t *src) {
        return static_cast<uint32_t>(src[0]) |
               (static_cast<uint32_t>(src[1]) << 8) |
               (static_cast<uint32_t>(src[2]) << 16) |
               (static_cast<uint32_t>(src[3]) << 24);
    }

    bool buildFrame(const ProtocolConstants::FrameType type,
                      const void *payload,
                      const size_t payloadSize,
                      SerializedData& out
    ) {
        if (payloadSize + ProtocolConstants::PROTOCOL_OVERHEAD > ProtocolConstants::MAX_FRAME_SIZE) {
            LOG(LogLevel::ERROR, "Payload too large for frame");
            return false;
        }

        size_t offset = 0;

        out.data[offset++] = ProtocolConstants::encodeHeader(type);
        out.data[offset++] = static_cast<uint8_t>(payloadSize);

        memcpy(&out.data[offset], payload, payloadSize);
        offset += payloadSize;

        RawData rawData{out.data, offset};
        out.data[offset] = CRC8::compute(rawData);

        offset += ProtocolConstants::CRC_SIZE;
        out.size = offset;

        return true;
    }

public:
    SerializedData serializeCommand(const Command &cmd) override {
        uint8_t payload[ProtocolConstants::MAX_FRAME_SIZE];
        size_t payloadSize = CommandPacker::pack(cmd, payload);

        if (payloadSize == 0) {
            LOG(LogLevel::ERROR, "Failed to pack command");
            return SerializedData{};
        }

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::COMMAND, payload, payloadSize, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeCommand(const RawData &rawData, Command &cmdOut) override {
       auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::COMMAND);
        if (!frameInfo.valid){return false;}

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        return CommandPacker::unpack(&rawData.data[frameInfo.payloadStart], frameInfo.payloadLength, cmdOut);
    }

    bool extractCommandPayload(const RawData &frame, uint8_t *payloadOut, uint8_t &payloadSizeOut) override {
        auto frameInfo = validateFrameHeader(frame, ProtocolConstants::FrameType::COMMAND);
        if (!frameInfo.valid) return false;

        if (frame.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        const size_t crcOffset = frameInfo.payloadStart + frameInfo.payloadLength;
        if (!verifyCRC(frame, crcOffset)) return false;

        payloadSizeOut = frameInfo.payloadLength;
        memcpy(payloadOut, &frame.data[frameInfo.payloadStart], payloadSizeOut);
        return true;
    }

    SerializedData serializeResponse(const CommandResponse &response) override {
        uint8_t payload[sizeof(CommandResponse)];
        payload[0] = response.seqNum;
        writeUint16LE(&payload[1], response.commandType);
        payload[3] = static_cast<uint8_t>(response.status);

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::RESPONSE, payload, sizeof(CommandResponse), result)) {
            return result;
        }
        return SerializedData{};
    }

    bool deserializeResponse(const RawData &rawData, CommandResponse &responseOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::RESPONSE);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength != sizeof(CommandResponse)) {
            LOG(LogLevel::ERROR, "Invalid response payload size");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        const size_t crcOffset = frameInfo.payloadStart + frameInfo.payloadLength;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        const uint8_t* p = &rawData.data[frameInfo.payloadStart];
        responseOut.seqNum = p[0];
        responseOut.commandType = readUint16LE(&p[1]);
        responseOut.status = static_cast<ProtocolConstants::ResponseStatus>(p[3]);

        return true;
    }

    SerializedData serializeDiscovery(const DiscoveryResponse &resp) override {
        const size_t actualSize = 1 + (resp.moduleCount * sizeof(ModuleInfo));

        uint8_t payload[actualSize];
        payload[0] = resp.moduleCount;
        memcpy(&payload[1], resp.modules, resp.moduleCount * sizeof(ModuleInfo));

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::DISCOVERY, payload, actualSize, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeDiscovery(const RawData &rawData, DiscoveryResponse &respOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::DISCOVERY);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < 1) {
            LOG(LogLevel::ERROR, "Discovery payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        size_t offset = frameInfo.payloadStart;

        const uint8_t moduleCount = rawData.data[offset++];
        if (frameInfo.payloadLength != 1 + (moduleCount * sizeof(ModuleInfo))) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }
        const size_t crcOffset = offset + moduleCount * sizeof(ModuleInfo);
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        DiscoveryResponse temp;
        temp.moduleCount = moduleCount;
        memcpy(temp.modules, &rawData.data[offset], moduleCount * sizeof(ModuleInfo));
        respOut = temp;

        return true;
    }

    SerializedData serializeValue(const ValueSource &value) override {
        const size_t actualSize = ProtocolConstants::TYPE_AND_SIZE_BYTE + value.getDataSize();

        uint8_t compactPayload[actualSize];
        compactPayload[0] = value.getTypeAndSize();
        memcpy(&compactPayload[1], value.getData(), value.getDataSize());

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::VALUE_SOURCE, compactPayload, actualSize, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeValue(const RawData &rawData, ValueSource &valueOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::VALUE_SOURCE);
        if (!frameInfo.valid){ return false;}

        if (frameInfo.payloadLength < ProtocolConstants::TYPE_AND_SIZE_BYTE) {
            LOG(LogLevel::ERROR, "ValueSource payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        size_t offset = frameInfo.payloadStart;

        const uint8_t typeAndSize = rawData.data[offset++];

        ValueSource temp;
        temp.setTypeAndSizeRaw(typeAndSize);
        const size_t expectedSize = temp.getDataSize();

        if (frameInfo.payloadLength != ProtocolConstants::TYPE_AND_SIZE_BYTE + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        memcpy(temp.getDataMutable(), &rawData.data[offset], expectedSize);
        valueOut = temp;

        return true;
    }

    SerializedData serializeTelemetry(const TelemetryData &telemetry) override {
        const size_t valueSize = ProtocolConstants::TYPE_AND_SIZE_BYTE + telemetry.getDataSize();
        const size_t totalSize = sizeof(uint16_t) + sizeof(uint32_t) + valueSize;

        uint8_t payload[totalSize];
        size_t offset = 0;

        writeUint16LE(&payload[offset], telemetry.sourceID);
        offset += sizeof(uint16_t);

        writeUint32LE(&payload[offset], telemetry.timestamp);
        offset += sizeof(uint32_t);

        payload[offset++] = telemetry.getTypeAndSize();
        memcpy(&payload[offset], telemetry.getData(), telemetry.getDataSize());

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::TELEMETRY, payload, totalSize, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeTelemetry(const RawData &rawData, TelemetryData &telemetryOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::TELEMETRY);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < sizeof(uint16_t) + sizeof(uint32_t) + ProtocolConstants::TYPE_AND_SIZE_BYTE) {
            LOG(LogLevel::ERROR, "Telemetry payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        size_t offset = frameInfo.payloadStart;

        const uint16_t sourceID = readUint16LE(&rawData.data[offset]);
        offset += sizeof(uint16_t);

        const uint32_t timestamp = readUint32LE(&rawData.data[offset]);
        offset += sizeof(uint32_t);

        const uint8_t typeAndSize = rawData.data[offset++];

        ValueSource temp;
        temp.setTypeAndSizeRaw(typeAndSize);
        const size_t expectedSize = temp.getDataSize();

        if (frameInfo.payloadLength != sizeof(uint16_t) + sizeof(uint32_t) + ProtocolConstants::TYPE_AND_SIZE_BYTE + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        TelemetryData result;
        result.sourceID = sourceID;
        result.timestamp = timestamp;
        result.setTypeAndSizeRaw(typeAndSize);
        memcpy(result.getDataMutable(), &rawData.data[offset], expectedSize);
        telemetryOut = result;

        return true;

    }

    SerializedData serializeSettings(const SettingsData &settings) override {
        const size_t valueSize = ProtocolConstants::TYPE_AND_SIZE_BYTE + settings.getDataSize();
        const size_t totalSize = sizeof(uint16_t) + valueSize;

        uint8_t payload[totalSize];
        size_t offset = 0;

        writeUint16LE(&payload[offset], settings.settingsID);
        offset += sizeof(uint16_t);

        payload[offset++] = settings.getTypeAndSize();
        memcpy(&payload[offset], settings.getData(), settings.getDataSize());

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::SETTINGS, payload, totalSize, result)) {
            return result;
        }

        return  SerializedData{};
    }

    bool deserializeSettings(const RawData &rawData, SettingsData &settingsOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::SETTINGS);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < sizeof(uint16_t) + ProtocolConstants::TYPE_AND_SIZE_BYTE) {
            LOG(LogLevel::ERROR, "Settings payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        size_t offset = frameInfo.payloadStart;

        const uint16_t settingsID = readUint16LE(&rawData.data[offset]);
        offset += sizeof(uint16_t);

       const uint8_t typeAndSize = rawData.data[offset++];

        ValueSource temp;
        temp.setTypeAndSizeRaw(typeAndSize);
        const size_t expectedSize = temp.getDataSize();

        if (frameInfo.payloadLength != sizeof(uint16_t) + ProtocolConstants::TYPE_AND_SIZE_BYTE + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        SettingsData result;
        result.settingsID = settingsID;
        result.setTypeAndSizeRaw(typeAndSize);
        memcpy(result.getDataMutable(), &rawData.data[offset], expectedSize);

        settingsOut = result;
        return true;
    }

    uint8_t computeIntegrityCode(const RawData &rawData) override {
        return CRC8::compute(rawData);
    }
};

#endif //SMARTDRIVE_BINARYENCODER_H
