//
// Created by dunamis on 30/01/2026.
//

#ifndef SMARTDRIVE_PROTOCOLCONSTANTS_H
#define SMARTDRIVE_PROTOCOLCONSTANTS_H

#include "../core/platform.h"

namespace ProtocolConstants
{
    constexpr uint8_t STX_PATTERN = 0x02;
    constexpr uint8_t STX_MASK = 0x1F;        // Lower 5 bits
    constexpr uint8_t TYPE_SHIFT = 5;         // Upper 3 bits for type
    constexpr uint8_t CRC_SIZE = 1;           // 1 byte is used for CRC
    constexpr uint8_t TYPE_AND_SIZE_BYTE = 1; // 1 byte for ValueSource type and size

    constexpr uint16_t MAX_PAYLOAD_SIZE = 64;
    constexpr uint8_t CRC_OFFSET = 1;
    constexpr uint8_t NOP_BYTE = 0x00; // Sent when clocking in data with nothing to transmit

    constexpr uint16_t PROTOCOL_OVERHEAD = 3; // STX+TYPE(1) + LENGTH(1) + CRC(1)
    constexpr uint16_t MAX_FRAME_SIZE = MAX_PAYLOAD_SIZE + PROTOCOL_OVERHEAD;

    constexpr uint8_t SEQ_NUM_FIRE_AND_FORGET = 0x00;
    constexpr uint8_t SEQ_NUM_MIN = 0x01;
    constexpr uint8_t SEQ_NUM_MAX = 0xFF;

    enum class FrameType : uint8_t
    {
        COMMAND = 0x00,
        RESPONSE = 0x01,
        TELEMETRY = 0x02
    };

    enum class ResponseStatus : uint8_t
    {
        OK = 0x00,
        UNKNOWN_COMMAND_TYPE = 0X01,
        INVALID_PARAMS = 0X02,
        HARDWARE_BUSY = 0X03,
        HARDWARE_FAULT = 0X04,
        NOT_SUPPORTED = 0X05,
    };

    constexpr uint8_t encodeHeader(FrameType type)
    {
        return (static_cast<uint8_t>(type) << TYPE_SHIFT) | STX_PATTERN;
    }

    constexpr FrameType decodeType(const uint8_t header)
    {
        return static_cast<FrameType>(header >> TYPE_SHIFT);
    }

    constexpr bool isValidHeader(const uint8_t header)
    {
        return (header & STX_MASK) == STX_PATTERN;
    }

    constexpr bool isValidFrameType(const uint8_t header)
    {
        return (header >> TYPE_SHIFT) <= static_cast<uint8_t>(FrameType::TELEMETRY);
    }
}

#endif // SMARTDRIVE_PROTOCOLCONSTANTS_H