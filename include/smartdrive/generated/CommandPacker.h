//
// Created by dunamis on 16/02/2026.
//

#ifndef SMARTDRIVE_COMMANDPACKER_H
#define SMARTDRIVE_COMMANDPACKER_H

#include "../types/ProtocolTypes.h"
#include <cstdint>

class CommandPacker {
public:
    static size_t pack(const Command& cmd, uint8_t* buffer) {
        size_t offset = 0;

        buffer[offset++] = cmd.commandType & 0xFF;
        buffer[offset++] = (cmd.commandType >> 8) & 0xFF;

        switch (cmd.commandType) {
            case 0x0001: {
                return offset;
            }

            case 0x0002: {
                std::memcpy(&buffer[offset], cmd.params[0].getData(), 4);
                return offset + sizeof(float);
            }

            default:
                return 0;
        }
    }

    static bool unpack(const uint8_t* buffer, size_t bufferSize, Command& cmdOut) {
        if (bufferSize < 2) return false;

        size_t offset = 0;
        uint16_t cmdType = buffer[offset] | (buffer[offset + 1] << 8);
        offset += 2;

        cmdOut.commandType = cmdType;

        switch (cmdType) {
            case 0x0001: {
                return true;
            }
            case 0x0002: {
                if (bufferSize < offset + 4) return false;

                cmdOut.params[0] = 0.0f;
                std::memcpy(cmdOut.params[0].getDataMutable(), &buffer[offset], 4);
                return true;
            }
            default:
                return false;
        }
    }
};

#endif //SMARTDRIVE_COMMANDPACKER_H