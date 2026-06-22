//
// Created by dunamis on 16/02/2026.
//

#include <iostream>
#include "smartdrive/registry/CommandRegistry.h"
#include "smartdrive/generated/CommandTypes.h"
#include "smartdrive/generated/CommandPacker.h"

int main() {
    std::cout << "=== Indicator Board Command Test ===" << std::endl;

    // Test Registry
    CommandRegistry registry;
    registry.initialize();

    // Test 1: OLED_PRINT_STR
    std::cout << "\n--- Test 1: OLED_PRINT_STR ---" << std::endl;

    if (auto cmdInfo = registry.getCommandInfo(CommandType::OLED_PRINT_STR)) {
        std::cout << "✓ Found OLED_PRINT_STR: " << cmdInfo->description << std::endl;
        std::cout << "  Params: " << cmdInfo->params.size() << std::endl;
        for (const auto& param : cmdInfo->params) {
            std::cout << "    - " << param.name << " ("
                      << typeToString(param.type) << "): "
                      << param.description << std::endl;
        }
    }

    Command cmd1;
    cmd1.commandType = CommandType::OLED_PRINT_STR;
    cmd1.params[0] = uint16_t(10);   // x coordinate
    cmd1.params[1] = uint16_t(20);   // y coordinate
    cmd1.params[2] = "Hello World";  // text

    uint8_t buffer1[64];
    size_t size1 = CommandPacker::pack(cmd1, buffer1);
    std::cout << "✓ Packed OLED_PRINT_STR command: " << size1 << " bytes" << std::endl;

    // Print hex dump
    std::cout << "  Hex: ";
    for (size_t i = 0; i < size1; i++) {
        if (buffer1[i] < 0x10) std::cout << "0";
        std::cout << std::hex << (int)buffer1[i] << " ";
    }
    std::cout << std::dec << std::endl;

    Command received1;
    if (CommandPacker::unpack(buffer1, size1, received1)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  X = " << uint16_t(received1.params[0]) << std::endl;
        std::cout << "  Y = " << uint16_t(received1.params[1]) << std::endl;
        std::cout << "  Text = \"" << received1.params[2].getData() << "\"" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    // Test 2: OLED_DRAW_BAR
    std::cout << "\n--- Test 2: OLED_DRAW_BAR ---" << std::endl;

    if (auto cmdInfo = registry.getCommandInfo(CommandType::OLED_DRAW_BAR)) {
        std::cout << "✓ Found OLED_DRAW_BAR: " << cmdInfo->description << std::endl;
        std::cout << "  Params: " << cmdInfo->params.size() << std::endl;
    }

    Command cmd2;
    cmd2.commandType = CommandType::OLED_DRAW_BAR;
    cmd2.params[0] = uint16_t(5);    // x coordinate
    cmd2.params[1] = uint16_t(50);   // y coordinate
    cmd2.params[2] = uint8_t(75);    // percent (75%)

    uint8_t buffer2[64];
    size_t size2 = CommandPacker::pack(cmd2, buffer2);
    std::cout << "✓ Packed OLED_DRAW_BAR command: " << size2 << " bytes" << std::endl;

    Command received2;
    if (CommandPacker::unpack(buffer2, size2, received2)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        std::cout << "  X = " << uint16_t(received2.params[0]) << std::endl;
        std::cout << "  Y = " << uint16_t(received2.params[1]) << std::endl;
        std::cout << "  Percent = " << int(uint8_t(received2.params[2])) << "%" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    // Test 3: LED_SET_BLOCK
    std::cout << "\n--- Test 3: LED_SET_BLOCK ---" << std::endl;

    if (auto cmdInfo = registry.getCommandInfo(CommandType::LED_SET_BLOCK)) {
        std::cout << "✓ Found LED_SET_BLOCK: " << cmdInfo->description << std::endl;
        std::cout << "  Params: " << cmdInfo->params.size() << std::endl;
    }

    Command cmd3;
    cmd3.commandType = CommandType::LED_SET_BLOCK;
    cmd3.params[0] = uint16_t(0xF800);  // RGB565 Red (11111 000000 00000)

    uint8_t buffer3[64];
    size_t size3 = CommandPacker::pack(cmd3, buffer3);
    std::cout << "✓ Packed LED_SET_BLOCK command: " << size3 << " bytes" << std::endl;

    Command received3;
    if (CommandPacker::unpack(buffer3, size3, received3)) {
        std::cout << "✓ Unpacked successfully" << std::endl;
        uint16_t color = uint16_t(received3.params[0]);
        std::cout << "  Color = 0x" << std::hex << color << std::dec;
        std::cout << " (RGB565)" << std::endl;
    } else {
        std::cout << "✗ Failed to unpack" << std::endl;
    }

    // Test 4: Size comparison
    std::cout << "\n--- Size Comparison ---" << std::endl;
    std::cout << "OLED_PRINT_STR: " << size1 << " bytes (has STRING)" << std::endl;
    std::cout << "OLED_DRAW_BAR:  " << size2 << " bytes (fixed size)" << std::endl;
    std::cout << "LED_SET_BLOCK:  " << size3 << " bytes (fixed size)" << std::endl;

    // Test 5: Common RGB565 colors
    std::cout << "\n--- RGB565 Color Reference ---" << std::endl;
    std::cout << "Red:     0xF800 (11111 000000 00000)" << std::endl;
    std::cout << "Green:   0x07E0 (00000 111111 00000)" << std::endl;
    std::cout << "Blue:    0x001F (00000 000000 11111)" << std::endl;
    std::cout << "White:   0xFFFF (11111 111111 11111)" << std::endl;
    std::cout << "Black:   0x0000 (00000 000000 00000)" << std::endl;
    std::cout << "Yellow:  0xFFE0 (11111 111111 00000)" << std::endl;
    std::cout << "Cyan:    0x07FF (00000 111111 11111)" << std::endl;
    std::cout << "Magenta: 0xF81F (11111 000000 11111)" << std::endl;

    return 0;
}