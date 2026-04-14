//
// Created by dunamis on 29/01/2026.
//

#ifndef SMARTDRIVE_PROTOCOLTYPES_H
#define SMARTDRIVE_PROTOCOLTYPES_H

#include "../core/Config.h"
#include "../core/ValueSource.h"
#include "../constants/ProtocolConstants.h"

struct RawData
{
    uint8_t *data;
    size_t size;
};

struct SerializedData
{
    uint8_t data[ProtocolConstants::MAX_FRAME_SIZE];
    size_t size = 0;
};

#pragma pack(push, 1)

struct Command
{
    uint16_t commandType;
    ValueSource params[3];
};

struct CommandResponse
{
    uint8_t seqNum;
    uint16_t commandType;
    ProtocolConstants::ResponseStatus status;
};

#pragma pack(pop)

static_assert(sizeof(Command) == 53, "Command must be exactly 53 bytes");
static_assert(sizeof(CommandResponse) == 4, "CommandResponse must be exactly 4 bytes");

#endif // SMARTDRIVE_PROTOCOLTYPES_H
