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

    std::cout << "\n--- Test 3: LOG_MESSAGE with timestamp and message ---" << std::endl;
    Command cmd3;
    cmd3.commandType = CommandType::LOG_MESSAGE;
    cmd3.params[0] = 1890.0f;
    cmd3.params[1] = "Hello wertghuhjgfdssdg";

    uint8_t buffer3[64];
    size_t size3 = CommandPacker::pack(cmd3, buffer3);
    std::cout << "✓ Packed LOG_MESSAGE command: " << size3 << " bytes" << std::endl;

    Command received3;
    if (CommandPacker::unpack(buffer3, size3, received3)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  Timestamp = " << float(received3.params[0]) << std::endl;
        std::cout << "  Message = " << received3.params[1].getData() << " (user-provided)" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    std::cout << "\n--- Test 4: SET_LABEL with label and weight ---" << std::endl;
    Command cmd4;
    cmd4.commandType = CommandType::SET_LABEL;
    cmd4.params[0] = "Testhhhehehehbrueue3hrjenjenjnfjnef";
    cmd4.params[1] = 12.4f;

    uint8_t buffer4[64];
    size_t size4 = CommandPacker::pack(cmd4, buffer4);
    if (size4 > 0) {
        std::cout << "✓ Packed SET_LABEL command: " << size4 << " bytes" << std::endl;
    }

    Command received4;
    if (CommandPacker::unpack(buffer4, size4, received4)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  Label = " << received4.params[0].getData() << std::endl;
        std::cout << "  Value = " << float(received4.params[1]) << std::endl;
    }else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    return 0;
}