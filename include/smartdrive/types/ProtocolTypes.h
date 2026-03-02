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

struct ModuleInfo
{
    uint16_t typeID;    // Component type (e.g. indicator board, servo controller)
    uint8_t instanceID; // Serial number / instance identifier
    uint32_t capabilitiesBitmask;
    uint8_t protocolRevision; // CDnC protocol revision this component speaks
};

struct DiscoveryResponse
{
    uint8_t moduleCount;
    ModuleInfo modules[MAX_NUM_MODULES];
};

#pragma pack(pop)

static_assert(sizeof(Command) == 53, "Command must be exactly 53 bytes");
static_assert(sizeof(ModuleInfo) == 8, "ModuleInfo must be exactly 8 bytes");
static_assert(sizeof(DiscoveryResponse) == ((8 * MAX_NUM_MODULES) + 1), "DiscoveryResponse has invalid size");

#endif // SMARTDRIVE_PROTOCOLTYPES_H
