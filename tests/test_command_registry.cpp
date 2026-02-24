z//
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

    // STOP command test
    if (auto stopCmd = registry.getCommandInfo(CommandType::STOP)) {
        std::cout << "✓ Found STOP: " << stopCmd->description << std::endl;
        std::cout << "  Params: " << stopCmd->params.size() << std::endl;
    } else {
        std::cout << "✗ STOP command not found" << std::endl;
    }

    // MOVE command test
    if (auto moveCmd = registry.getCommandInfo(CommandType::MOVE)) {
        std::cout << "✓ Found MOVE: " << moveCmd->description << std::endl;
        std::cout << "  Params: " << moveCmd->params.size() << std::endl;
        for (const auto& param : moveCmd->params) {
            std::cout << "    - " << param.name << " ("
                      << typeToString(param.type) << "): "
                      << param.description;
            if (!param.required) {
                std::cout << " (optional, default: " << param.defaultValue << ")";
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << "✗ MOVE command not found" << std::endl;
    }

    // Test Packer - MOVE with only required parameter
    std::cout << "\n--- Test 1: MOVE with only distance (no speed) ---" << std::endl;
    Command cmd1;
    cmd1.commandType = CommandType::MOVE;
    cmd1.params[0] = 10.5f;  // distance
    // params[1] (speed) not set - should use default

    uint8_t buffer1[64];
    size_t size1 = CommandPacker::pack(cmd1, buffer1);

    std::cout << "✓ Packed MOVE command: " << size1 << " bytes" << std::endl;

    // Unpack
    Command received1;
    if (CommandPacker::unpack(buffer1, size1, received1)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  Distance = " << float(received1.params[0]) << std::endl;
        std::cout << "  Speed = " << float(received1.params[1]) << " (should be default 1.0)" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    // Test Packer - MOVE with both parameters
    std::cout << "\n--- Test 2: MOVE with distance and speed ---" << std::endl;
    Command cmd2;
    cmd2.commandType = CommandType::MOVE;
    cmd2.params[0] = 20.3f;  // distance
    cmd2.params[1] = 2.5f;   // speed (optional, but provided)

    uint8_t buffer2[64];
    size_t size2 = CommandPacker::pack(cmd2, buffer2);

    std::cout << "✓ Packed MOVE command: " << size2 << " bytes" << std::endl;

    // Unpack
    Command received2;
    if (CommandPacker::unpack(buffer2, size2, received2)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  Distance = " << float(received2.params[0]) << std::endl;
        std::cout << "  Speed = " << float(received2.params[1]) << " (user-provided)" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    // Test size difference
    std::cout << "\n--- Size Comparison ---" << std::endl;
    std::cout << "MOVE without optional: " << size1 << " bytes" << std::endl;
    std::cout << "MOVE with optional:    " << size2 << " bytes" << std::endl;
    std::cout << "Savings: " << (size2 - size1) << " bytes when optional not sent" << std::endl;

    return 0;
}