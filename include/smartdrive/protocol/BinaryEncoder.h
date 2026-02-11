//
// Created by dunamis on 28/01/2026.
//

#ifndef SMARTDRIVE_BINARYENCODER_H
#define SMARTDRIVE_BINARYENCODER_H

#include <array>
#include <cstring>
#include "../interfaces/IEncoder.h"
#include "../types/ProtocolTypes.h"
#include "../types/RobotData.h"
#include "../utils/Logger.h"
#include "../core/Config.h"

#if DEBUG_ENABLED
#define BUFFER_ACCESS(buf, idx) (buf).at(idx)
#else
#define BUFFER_ACCESS(buf, idx) (buf)[idx]
#endif


class BinaryEncoder : public IEncoder {
private:
    std::array<uint8_t, ProtocolConstants::MAX_FRAME_SIZE> frameBuffer;

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

    static uint16_t calculateCRC16(const RawData &rawData) {
        uint16_t crc = 0xFFFF;

        for (size_t i = 0; i < rawData.size; ++i) {
            crc ^= static_cast<uint16_t>(rawData.data[i]) << 8;

            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    bool verifyCRC(const RawData &rawData, size_t crcOffset) const {
        const uint16_t receivedCrc = readUint16LE(&rawData.data[crcOffset]);

        const RawData crcData = {rawData.data, crcOffset};

        if (const uint16_t calculatedCRC = calculateCRC16(crcData);
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

    size_t buildFrame(const ProtocolConstants::FrameType type,
                      const void *payload,
                      const size_t payloadSize
    ) {
        size_t offset = 0;

        BUFFER_ACCESS(frameBuffer, offset) = ProtocolConstants::encodeHeader(type);
        offset++;

        BUFFER_ACCESS(frameBuffer, offset) = static_cast<uint8_t>(payloadSize);
        offset++;

        memcpy(&BUFFER_ACCESS(frameBuffer, offset), payload, payloadSize);
        offset += payloadSize;

        RawData rawData{};
        rawData.data = frameBuffer.data();
        rawData.size = offset;

        const uint16_t crc = calculateCRC16(rawData);
        writeUint16LE(&BUFFER_ACCESS(frameBuffer, offset), crc);
        offset += 2;

        return offset;
    }

    bool parseFrame(const RawData &rawData,
                    const ProtocolConstants::FrameType expectedType,
                    void *payloadOut,
                    const size_t expectedPayloadSize) const {
        if (rawData.size < ProtocolConstants::PROTOCOL_OVERHEAD) {
            LOG(LogLevel::ERROR, "Frame too small");
            return false;
        }

        size_t offset = 0;

        const uint8_t header = rawData.data[offset];
        offset++;

        if (!ProtocolConstants::isValidHeader(header)) {
            LOG(LogLevel::ERROR, "Invalid header");
            return false;
        }

        if (const ProtocolConstants::FrameType frameType = ProtocolConstants::decodeType(header);
            frameType != expectedType) {
            LOG(LogLevel::ERROR, "Frame type mismatch");
            return false;
        }

        const uint8_t payloadLength = rawData.data[offset];
        offset++;

        if (rawData.size != offset + payloadLength + 2) {//+2 is for the crc
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        if (payloadLength != expectedPayloadSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + payloadLength;
        const uint16_t receivedCrc = readUint16LE(&rawData.data[crcOffset]);

        RawData data;
        data.size = crcOffset;
        data.data = rawData.data;
        if (const uint16_t calculatedCRC = calculateCRC16(data); receivedCrc != calculatedCRC) {
            LOG(LogLevel::ERROR, "CRC mismatch");
            return false;
        }

        memcpy(payloadOut, &rawData.data[offset], payloadLength);
        return true;
    }

public:
    BinaryEncoder() {
        frameBuffer.fill(0);
    }

    SerializedData serializeCommand(const Command &cmd) override {
        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::COMMAND,
                                 &cmd, sizeof(Command));
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeCommand(const RawData &rawData, Command &cmdOut) override {
        return parseFrame(rawData,
                          ProtocolConstants::FrameType::COMMAND,
                          &cmdOut,
                          sizeof(Command));
    }

    SerializedData serializeDiscovery(const DiscoveryResponse &resp) override {
        const size_t actualSize = 1 + (resp.moduleCount * sizeof(ModuleInfo));

        uint8_t payload[actualSize];
        payload[0] = resp.moduleCount;
        std::memcpy(&payload[1], resp.modules, resp.moduleCount * sizeof(ModuleInfo));

        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::DISCOVERY, payload, actualSize);
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeDiscovery(const RawData &rawData, DiscoveryResponse &respOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::DISCOVERY);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < 1) {
            LOG(LogLevel::ERROR, "Discovery payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + 2) {
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
        std::memcpy(temp.modules, &rawData.data[offset], moduleCount * sizeof(ModuleInfo));
        respOut = temp;

        return true;
    }

    SerializedData serializeValue(const ValueSource &value) override {
        const size_t actualSize = 1 + value.getDataSize(); //+1 because of the typeAndSize byte

        uint8_t compactPayload[actualSize];
        compactPayload[0] = value.getTypeAndSize();
        std::memcpy(&compactPayload[1], value.getData(), value.getDataSize());

        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::VALUE_SOURCE, compactPayload, actualSize);
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeValue(const RawData &rawData, ValueSource &valueOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::VALUE_SOURCE);
        if (!frameInfo.valid){ return false;}

        if (frameInfo.payloadLength < 1) {
            LOG(LogLevel::ERROR, "ValueSource payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + 2) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        size_t offset = frameInfo.payloadStart;

        const uint8_t typeAndSize = rawData.data[offset++];

        ValueSource temp;
        temp.setTypeAndSizeRaw(typeAndSize);
        const size_t expectedSize = temp.getDataSize();

        if (frameInfo.payloadLength != 1 + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        std::memcpy(temp.getDataMutable(), &rawData.data[offset], expectedSize);
        valueOut = temp;

        return true;
    }

    SerializedData serializeTelemetry(const TelemetryData &telemetry) override {
       const size_t valueSize = 1 + telemetry.getDataSize(); //1 is for the typeAndSize byte in ValueSource
        const size_t totalSize = sizeof(uint16_t) + sizeof(uint32_t) + valueSize;

        uint8_t payload[totalSize];
       size_t offset = 0;

        writeUint16LE(&payload[offset], telemetry.sourceID);
        offset += sizeof(uint16_t);

        writeUint32LE(&payload[offset], telemetry.timestamp);
        offset += sizeof(uint32_t);

        payload[offset++] = telemetry.getTypeAndSize();
        std::memcpy(&payload[offset], telemetry.getData(), telemetry.getDataSize());

        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::TELEMETRY, payload, totalSize);

        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }

        return result;
    }

    bool deserializeTelemetry(const RawData &rawData, TelemetryData &telemetryOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::TELEMETRY);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < 7) {
            LOG(LogLevel::ERROR, "Telemetry payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + 2) {
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

        if (frameInfo.payloadLength != sizeof(uint16_t) + sizeof(uint32_t) + 1 + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        TelemetryData result;
        result.sourceID = sourceID;
        result.timestamp = timestamp;
        result.setTypeAndSizeRaw(typeAndSize);
        std::memcpy(result.getDataMutable(), &rawData.data[offset], expectedSize);
        telemetryOut = result;

        return true;

    }

    SerializedData serializeSettings(const SettingsData &settings) override {
        const size_t valueSize = 1 + settings.getDataSize(); //1 is for the typeAndSize byte
        const size_t totalSize = sizeof(uint16_t) + valueSize;

        uint8_t payload[totalSize];
        size_t offset = 0;

        writeUint16LE(&payload[offset], settings.settingsID);
        offset += sizeof(uint16_t);

        payload[offset++] = settings.getTypeAndSize();
        std::memcpy(&payload[offset], settings.getData(), settings.getDataSize());

        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::SETTINGS, payload, totalSize);

        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }

        return result;
    }

    bool deserializeSettings(const RawData &rawData, SettingsData &settingsOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::SETTINGS);
        if (!frameInfo.valid){return false;}

        if (frameInfo.payloadLength < 3) {
            LOG(LogLevel::ERROR, "Settings payload too small");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + 2) {
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

        if (frameInfo.payloadLength != sizeof(uint16_t) + 1 + expectedSize) {
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        if (!verifyCRC(rawData, crcOffset)) {return false;}

        SettingsData result;
        result.settingsID = settingsID;
        result.setTypeAndSizeRaw(typeAndSize);
        std::memcpy(result.getDataMutable(), &rawData.data[offset], expectedSize);

        settingsOut = result;
        return true;
    }

    uint16_t computeIntegrityCode(const RawData &rawData) override {
        return calculateCRC16(rawData);
    }
};

#endif //SMARTDRIVE_BINARYENCODER_H
