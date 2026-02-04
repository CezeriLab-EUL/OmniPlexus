//
// Created by dunamis on 29/01/2026.
//

#ifndef SMARTDRIVE_PROTOCOLTYPES_H
#define SMARTDRIVE_PROTOCOLTYPES_H

#include <cstdint>
#include "../core/Config.h"

struct RawData {
    uint8_t* data ;
    size_t size;
};


#pragma pack(push, 1)

struct Command {
    uint16_t commandType;
    float w;
    float x;
    float  y;
    float  z;
    int16_t s;
    int16_t t;
    int16_t u;
    int16_t v;
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

static_assert(sizeof(Command) == 26, "Command must be exactly 26 bytes");
static_assert(sizeof(ModuleInfo) == 7, "ModuleInfo must be exactly 7 bytes");
static_assert(sizeof(DiscoveryResponse) == ((7*MAX_NUM_MODULES)+1), "DiscoveryResponse has invalid size");

#endif //SMARTDRIVE_PROTOCOLTYPES_H
