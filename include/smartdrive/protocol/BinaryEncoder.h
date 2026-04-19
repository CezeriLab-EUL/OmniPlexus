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
            LOG(LogLevel::OP_ERROR, "Frame too small");
            return result;
        }

        size_t offset = 0;

        const uint8_t header = rawData.data[offset++];
        if (!ProtocolConstants::isValidHeader(header)) {
            LOG(LogLevel::OP_ERROR, "Invalid header");
            return result;
        }

        if (const ProtocolConstants::FrameType frameType = ProtocolConstants::decodeType(header);
            frameType != expectedType) {
            LOG(LogLevel::OP_ERROR, "Frame type mismatch");
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
            LOG(LogLevel::OP_ERROR, "CRC mismatch");
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
            LOG(LogLevel::OP_ERROR, "Payload too large for frame");
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
    SerializedData serializeCommand(const Command &cmd, uint8_t seqNum) override {
        uint8_t payload[ProtocolConstants::MAX_FRAME_SIZE];
        size_t offset = 0;

        payload[offset++] = seqNum;

        size_t payloadSize = CommandPacker::pack(cmd, &payload[offset]);
        if (payloadSize == 0) {
            LOG(LogLevel::OP_ERROR, "Failed to pack command");
            return SerializedData{};
        }
        offset += payloadSize;

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::COMMAND, payload, offset, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeCommand(const RawData &rawData, Command &cmdOut) override {
       auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::COMMAND);
        if (!frameInfo.valid){return false;}

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::OP_ERROR, "Invalid frame size");
            return false;
        }

        return CommandPacker::unpack(&rawData.data[frameInfo.payloadStart + 1],
            frameInfo.payloadLength - 1, cmdOut);
    }

    bool extractCommandPayload(const RawData &frame, uint8_t *payloadOut, uint8_t &payloadSizeOut, uint8_t &seqNumOut) override {
        auto frameInfo = validateFrameHeader(frame, ProtocolConstants::FrameType::COMMAND);
        if (!frameInfo.valid) return false;

        if (frame.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::OP_ERROR, "Invalid frame size");
            return false;
        }

        const size_t crcOffset = frameInfo.payloadStart + frameInfo.payloadLength;
        if (!verifyCRC(frame, crcOffset)) return false;

        seqNumOut = frame.data[frameInfo.payloadStart];
        payloadSizeOut = frameInfo.payloadLength - 1; //-1 because we are not storing seqNum
        memcpy(payloadOut, &frame.data[frameInfo.payloadStart + 1], payloadSizeOut); //+1 becuase we dont want to copy tje seqNum
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
            LOG(LogLevel::OP_ERROR, "Invalid response payload size");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::OP_ERROR, "Invalid frame size");
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

    SerializedData serializeTelemetry(const Telemetry &telemetry) override {
        uint8_t payload[ProtocolConstants::MAX_PAYLOAD_SIZE];
        size_t offset = 0;

        writeUint16LE(&payload[offset], telemetry.sourceID);
        offset += 2;

        payload[offset++] = telemetry.getTypeAndSize();

        const size_t dataSize = telemetry.getDataSize();
        memcpy(&payload[offset], telemetry.getData(), dataSize);
        offset += dataSize;

        if (SerializedData result; buildFrame(ProtocolConstants::FrameType::TELEMETRY, payload, offset, result)) {
            return result;
        }

        return SerializedData{};
    }

    bool deserializeTelemetry(const RawData &rawData, Telemetry &telemetryOut) override {
        auto frameInfo = validateFrameHeader(rawData, ProtocolConstants::FrameType::TELEMETRY);
        if (!frameInfo.valid) { return false; }

        if (frameInfo.payloadLength < 3) { // Minimum size: 2 bytes for sourceID + 1 byte for typeAndSize
            LOG(LogLevel::OP_ERROR, "Invalid telemetry payload size");
            return false;
        }

        if (rawData.size != frameInfo.payloadStart + frameInfo.payloadLength + ProtocolConstants::CRC_SIZE) {
            LOG(LogLevel::OP_ERROR, "Invalid frame size");
            return false;
        }

        const size_t crcOffset = frameInfo.payloadStart + frameInfo.payloadLength;
        if (!verifyCRC(rawData, crcOffset)) { return false; }

        const uint8_t* p = &rawData.data[frameInfo.payloadStart];

        telemetryOut.sourceID = readUint16LE(p);
        telemetryOut.setTypeAndSizeRaw(p[2]);

        const size_t dataSize = telemetryOut.getDataSize();
        if (dataSize + 3 > frameInfo.payloadLength) {
            LOG(LogLevel::OP_ERROR, "Invalid telemetry data size");
            return false;
        }

        memcpy(telemetryOut.getDataMutable(), &p[3], dataSize);

        return true;
    }

    uint8_t computeIntegrityCode(const RawData &rawData) override {
        return CRC8::compute(rawData);
    }
};

#endif //SMARTDRIVE_BINARYENCODER_H
