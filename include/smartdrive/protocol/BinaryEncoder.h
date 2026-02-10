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
        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::DISCOVERY,
                                 &resp, sizeof(DiscoveryResponse));
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeDiscovery(const RawData &rawData, DiscoveryResponse &respOut) override {
        return parseFrame(rawData,
                          ProtocolConstants::FrameType::DISCOVERY,
                          &respOut,
                          sizeof(DiscoveryResponse));
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
        if (rawData.size < ProtocolConstants::PROTOCOL_OVERHEAD + 1) {
            LOG(LogLevel::ERROR, "Frame too small");
            return false;
        }

        size_t offset = 0;

        const uint8_t header = rawData.data[offset++];
        if (!ProtocolConstants::isValidHeader(header)) {
            LOG(LogLevel::ERROR, "Invalid header");
            return false;
        }

        if (const ProtocolConstants::FrameType frameType = ProtocolConstants::decodeType(header);
            frameType != ProtocolConstants::FrameType::VALUE_SOURCE) {
            LOG(LogLevel::ERROR, "Frame type mismatch");
            return false;
        }

        const uint8_t payloadLength = rawData.data[offset++];
        if (rawData.size != offset + payloadLength + 2) {
            LOG(LogLevel::ERROR, "Invalid frame size");
            return false;
        }

        const uint8_t typeAndSize = rawData.data[offset++];
        ValueSource temp;
        temp.setTypeAndSizeRaw(typeAndSize);
        const size_t expectedSize = temp.getDataSize();

        if (payloadLength != 1 + expectedSize) { // +1 because of the typeAndSize byte
            LOG(LogLevel::ERROR, "Payload size mismatch");
            return false;
        }

        const size_t crcOffset = offset + expectedSize;
        const uint16_t receivedCrc = readUint16LE(&rawData.data[crcOffset]);

        RawData crcData;
        crcData.size = crcOffset;
        crcData.data = rawData.data;
        if (const uint16_t calculatedCRC = calculateCRC16(crcData); receivedCrc != calculatedCRC) {
            LOG(LogLevel::ERROR, "CRC mismatch");
            return false;
        }

        std::memcpy(temp.getDataMutable(), &rawData.data[offset], expectedSize);
        valueOut = temp;

        return true;
    }

    SerializedData serializeTelemetry(const TelemetryData &telemetry) override {
        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::TELEMETRY,
                                 &telemetry, sizeof(TelemetryData));
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeTelemetry(const RawData &rawData, TelemetryData &telemetryOut) override {
        return parseFrame(rawData,
                          ProtocolConstants::FrameType::TELEMETRY,
                          &telemetryOut,
                          sizeof(TelemetryData));
    }

    SerializedData serializeSettings(const SettingsData &settings) override {
        SerializedData result;
        result.size = buildFrame(ProtocolConstants::FrameType::SETTINGS, &settings, sizeof(SettingsData));
        if (result.size > 0) {
            memcpy(result.data, frameBuffer.data(), result.size);
        }
        return result;
    }

    bool deserializeSettings(const RawData &rawData, SettingsData &settingsOut) override {
        return parseFrame(rawData,
                          ProtocolConstants::FrameType::SETTINGS,
                          &settingsOut,
                          sizeof(SettingsData));
    }

    uint16_t computeIntegrityCode(const RawData &rawData) override {
        return calculateCRC16(rawData);
    }
};

#endif //SMARTDRIVE_BINARYENCODER_H
