//
// Created by dunamis on 07/03/2026.
//

#include <thread>

#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/core/CommunicationManager.h"
#include "smartdrive/transport/PcSerialTransport.h"
#include <csignal>

volatile bool running = true;

void signalHandler(int) {
    running = false;
}

void doSomething(const Command& cmd, void* context) {
    switch (cmd.commandType) {
        case CommandType::LED_SET_BLOCK:
            std::cout << "Received command: " << std::endl;
            std::cout << uint16_t(cmd.params[0]) << std::endl;
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    const std::string port     = "/dev/ttyUSB0";
    const uint32_t    baudRate = 115200;

    std::cout << "=== SmartDrive Raw Debug Test ===" << std::endl;
    std::cout << "Port: " << port << " @ " << baudRate << " baud" << std::endl;

    std::cout << "\n Opening serial port..." << std::endl;
    PcSerialTransport transport(port, baudRate);
    std::cout << "    Port opened successfully." << std::endl;

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &transport);
    cm.onCommandReceived(doSomething, nullptr);

    while (running) {
        cm.listen();
        cm.processCommands();
    }
}