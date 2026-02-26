//
// Created by dunamis on 25/02/2026.
//
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "smartdrive/transport/PcSerialTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/core/CommunicationManager.h"
#include "smartdrive/generated/IndicatorBoardController.h"

// Small helper to pause between commands so the Arduino has time to process
void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

int main() {
    const std::string port = "/dev/ttyUSB0";
    const uint32_t baudRate = 115200;

    std::cout << "SmartDrive LED Test" << std::endl;
    std::cout << "Connecting to " << port << " at " << baudRate << " baud..." << std::endl;

    PcSerialTransport transport(port, baudRate);
    BinaryEncoder encoder;
    CommunicationManager comms(&encoder, &transport);
    IndicatorBoardController board(comms);

    // Arduino needs a moment to reset after serial connection is established
    std::cout << "Waiting for Arduino to boot..." << std::endl;
    wait(3000);
    std::cout << "Ready!" << std::endl;

    // --- Test 1: Turn LED ON (white = 0xFFFF in RGB565) ---
    // std::cout << "Sending LED_SET_BLOCK: ON (white)..." << std::endl;
    // if (board.ledSetBlock(0xFFFF)) {
    //     std::cout << "Sent! LED should be ON." << std::endl;
    // } else {
    //     std::cout << "Failed to send command." << std::endl;
    //     return 1;
    // }
    //
    // wait(3000);

    // --- Test 2: Turn LED OFF (black = 0x0000 in RGB565) ---
    // std::cout << "Sending LED_SET_BLOCK: OFF (black)..." << std::endl;
    // if (board.ledSetBlock(0x0000)) {
    //     std::cout << "Sent! LED should be OFF." << std::endl;
    // } else {
    //     std::cout << "Failed to send command." << std::endl;
    //     return 1;
    // }
    //
    // wait(3000);

    // --- Test 3: Blink 5 times ---
    // std::cout << "Blinking 5 times..." << std::endl;
    // for (int i = 0; i < 5; i++) {
    //     board.ledSetBlock(0xFFFF);
    //     wait(500);
    //     board.ledSetBlock(0x0000);
    //     wait(500);
    // }

    // --- Test 4: Beep for 500ms ---
    // std::cout << "Sending BEEP for 500ms..." << std::endl;
    // if (board.beep(500)) {
    //     std::cout << "Sent! You should hear a beep." << std::endl;
    // } else {
    //     std::cout << "Failed to send command." << std::endl;
    //     return 1;
    // }
    //
    // wait(1000);

    //--- Test 5: Print text on OLED ---

    if (board.oledPrintStr(0, 10, "SmartDrive!")) {
        std::cout << "Sent! Text should appear on OLED." << std::endl;
    }else {
        std::cout << "Failed to send oledPrintStr command." << std::endl;
        return 1;
    }

    // std::cout << "Sending OLED_PRINT_STR..." << std::endl;
    // if (board.oledClear()) {
    //     if (board.oledSetFont(0)) {
    //         if (board.oledPrintStr(0, 10, "SmartDrive!")) {
    //             std::cout << "Sent! Text should appear on OLED." << std::endl;
    //         }else {
    //             std::cout << "Failed to send oledPrintStr command." << std::endl;
    //             return 1;
    //         }
    //     }else {
    //         std::cout << "Failed to send oledSetFont command." << std::endl;
    //         return 1;
    //     }
    // }else {
    //     std::cout << "Failed to send oledClear command." << std::endl;
    //     return 1;
    // }

    std::cout << "Test complete!" << std::endl;
    return 0;
}