//
// Created by dunamis on 30/01/2026.
//

#ifndef SMARTDRIVE_PROTOCOLCONSTANTS_H
#define SMARTDRIVE_PROTOCOLCONSTANTS_H

#include <cstdint>

namespace ProtocolConstants {
    constexpr uint8_t STX_PATTERN = 0x02;
    constexpr uint8_t STX_MASK = 0x1F; //Lower 5 bits
    constexpr uint8_t TYPE_SHIFT = 5; //Upper 3 bits for type

    constexpr uint16_t MAX_PAYLOAD_SIZE = 64;

    constexpr uint16_t PROTOCOL_OVERHEAD = 4; //STX+TYPE(1) + LENGTH(1) + CRC(2)
    constexpr uint16_t MAX_FRAME_SIZE = MAX_PAYLOAD_SIZE + PROTOCOL_OVERHEAD;

    enum class FrameType : uint8_t {
        COMMAND = 0x00,
        DISCOVERY = 0x01,
        TELEMETRY = 0x02,
        SETTINGS = 0x03,
        VALUE_SOURCE = 0x04
    };

    constexpr uint8_t encodeHeader(FrameType type) {
        return (static_cast<uint8_t>(type) << TYPE_SHIFT) | STX_PATTERN;
    }

    constexpr FrameType decodeType(const uint8_t header) {
        return static_cast<FrameType>(header >> TYPE_SHIFT);
    }

    constexpr bool isValidHeader(const uint8_t header) {
        return (header & STX_MASK) == STX_PATTERN;
    }
}

#endif //SMARTDRIVE_PROTOCOLCONSTANTS_H