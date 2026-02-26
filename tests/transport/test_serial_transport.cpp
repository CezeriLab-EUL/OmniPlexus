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
#include "smartdrive/generated/CommandTypes.h"
#include "smartdrive/types/ProtocolTypes.h"

// Small helper to pause between commands so the Arduino has time to process
void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

int main() {
    // ---- Change this to your Arduino's port ----
    // Run: ls /dev/ttyACM* or ls /dev/ttyUSB*  to find it
    const std::string port = "/dev/ttyUSB0";
    const uint32_t baudRate = 9600;
    // --------------------------------------------

    std::cout << "SmartDrive LED Test" << std::endl;
    std::cout << "Connecting to " << port << " at " << baudRate << " baud..." << std::endl;

    PcSerialTransport transport(port, baudRate);
    BinaryEncoder encoder;
    CommunicationManager comms(&encoder, &transport);

    // Arduino needs a moment to reset after serial connection is established
    std::cout << "Waiting for Arduino to boot..." << std::endl;
    wait(2000);
    std::cout << "Ready!" << std::endl;

    // --- Test 1: Turn LED ON (white = 0xFFFF in RGB565) ---
    std::cout << "Sending LED_SET_BLOCK: ON (white)..." << std::endl;
    Command cmdOn;
    cmdOn.commandType = CommandType::LED_SET_BLOCK;
    cmdOn.params[0] = uint16_t(0xFFFF);  // white = all bits set = full brightness

    if (comms.dispatch(cmdOn)) {
        std::cout << "Sent! LED should be ON." << std::endl;
    } else {
        std::cout << "Failed to send command." << std::endl;
        return 1;
    }

    wait(3000);  // keep LED on for 3 seconds

    // --- Test 2: Turn LED OFF (black = 0x0000 in RGB565) ---
    std::cout << "Sending LED_SET_BLOCK: OFF (black)..." << std::endl;
    Command cmdOff;
    cmdOff.commandType = CommandType::LED_SET_BLOCK;
    cmdOff.params[0] = uint16_t(0x0000);  // black = all bits clear = off

    if (comms.dispatch(cmdOff)) {
        std::cout << "Sent! LED should be OFF." << std::endl;
    } else {
        std::cout << "Failed to send command." << std::endl;
        return 1;
    }

    wait(3000);

    // --- Test 3: Blink 5 times ---
    std::cout << "Blinking 5 times..." << std::endl;
    for (int i = 0; i < 5; i++) {
        Command on;
        on.commandType = CommandType::LED_SET_BLOCK;
        on.params[0] = uint16_t(0xFFFF);
        comms.dispatch(on);
        wait(500);

        Command off;
        off.commandType = CommandType::LED_SET_BLOCK;
        off.params[0] = uint16_t(0x0000);
        comms.dispatch(off);
        wait(500);
    }

    std::cout << "Test complete!" << std::endl;
    return 0;
}