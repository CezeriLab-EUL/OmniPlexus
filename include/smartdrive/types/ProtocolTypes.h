//
// Created by dunamis on 29/01/2026.
//

#ifndef SMARTDRIVE_PROTOCOLTYPES_H
#define SMARTDRIVE_PROTOCOLTYPES_H

#include "../core/Config.h"
#include "../core/ValueSource.h"
#include "../constants/ProtocolConstants.h"

struct RawData {
    uint8_t* data ;
    size_t size;
};

struct SerializedData {
    uint8_t data[ProtocolConstants::MAX_FRAME_SIZE];
    size_t size = 0;
};


#pragma pack(push, 1)

struct Command {
    uint16_t commandType;
    ValueSource params[3];
};

struct ModuleInfo {
    uint16_t typeID;
    uint8_t instanceID;
    uint32_t capabilitiesBitmask;
};

struct DiscoveryResponse {
    uint8_t moduleCount;
    ModuleInfo modules[MAX_NUM_MODULES];
};

#pragma pack(pop)

static_assert(sizeof(Command) == 53, "Command must be exactly 26 bytes");
static_assert(sizeof(ModuleInfo) == 7, "ModuleInfo must be exactly 7 bytes");
static_assert(sizeof(DiscoveryResponse) == ((7*MAX_NUM_MODULES)+1), "DiscoveryResponse has invalid size");

#endif //SMARTDRIVE_PROTOCOLTYPES_H
