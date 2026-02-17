//
// Created by dunamis on 16/02/2026.
//

#include <iostream>
#include "smartdrive/registry/CommandRegistry.h"
#include "smartdrive/generated/CommandTypes.h"
#include "smartdrive/generated/CommandPacker.h"

int main() {
    std::cout << "=== Command Registry Test ===" << std::endl;

    // Test Registry
    CommandRegistry registry;
    registry.initialize();

    auto* stopCmd = registry.getCommandInfo(CommandType::STOP);
    if (stopCmd) {
        std::cout << "✓ Found STOP: " << stopCmd->description << std::endl;
        std::cout << "  Params: " << stopCmd->params.size() << std::endl;
    }

    auto* moveCmd = registry.getCommandInfo(CommandType::MOVE);
    if (moveCmd) {
        std::cout << "✓ Found MOVE: " << moveCmd->description << std::endl;
        std::cout << "  Params: " << moveCmd->params.size() << std::endl;
        for (const auto& param : moveCmd->params) {
            std::cout << "    - " << param.name << " ("
                      << typeToString(param.type) << "): "
                      << param.description << std::endl;
        }
    }

    // Test Packer
    Command cmd;
    cmd.commandType = CommandType::MOVE;
    cmd.params[0] = 10.5f;

    uint8_t buffer[64];
    size_t size = CommandPacker::pack(cmd, buffer);

    std::cout << "\n✓ Packed MOVE command: " << size << " bytes" << std::endl;

    // Unpack
    Command received;
    if (CommandPacker::unpack(buffer, size, received)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  Distance = " << float(received.params[0]) << std::endl;
    }

    return 0;
}