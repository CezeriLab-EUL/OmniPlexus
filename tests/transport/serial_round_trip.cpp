//
// Created by dunamis on 08/03/2026.
//

#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>

#include "smartdrive/transport/PcSerialTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/IndicatorBoardController.h"

volatile bool running = true;

void signalHandler(int) { running = false; }

void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void onCommandReceived(const Command& cmd, const uint8_t& seqNum,  void* context) {
    switch (cmd.commandType) {
        case CommandType::LED_SET_BLOCK: {
            uint16_t color = (uint16_t)cmd.params[0];
            std::cout << "[PC] Arduino echoed LED_SET_BLOCK with color: "
                      << color << std::endl;
            break;
        }
    }
}

int main() {
    std::signal(SIGINT, signalHandler);

    const std::string port    = "/dev/ttyUSB0";
    const uint32_t baudRate   = 115200;

    std::cout << "=== SmartDrive Bidirectional Test ===" << std::endl;
    std::cout << "Port: " << port << " @ " << baudRate << " baud" << std::endl;

    std::cout << "\nOpening serial port..." << std::endl;
    PcSerialTransport transport(port, baudRate);
    std::cout << "Port opened successfully." << std::endl;

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &transport);
    IndicatorBoardController device(cm);
    cm.onCommandReceived(onCommandReceived, nullptr);

    std::thread listenerThread([&cm]() {
        while (running) {
            cm.listen();
        }
    });

    std::thread processingThread([&cm]() {
        while (running) {
            cm.processCommands();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::cout << "\nWaiting for Arduino..." << std::endl;
    wait(6000);
    std::cout << "Arduino is ready." << std::endl;

    if (device.ledSetBlock(10)) {
        std::cout << "[PC] Sent LED_SET_BLOCK(10)" << std::endl;
    }
    wait(3000);

    if (device.ledSetBlock(0)) {
        std::cout << "[PC] Sent LED_SET_BLOCK(0)" << std::endl;
    }
    wait(3000);

    processingThread.join();
    listenerThread.join();
    running = false;
    return 0;
}