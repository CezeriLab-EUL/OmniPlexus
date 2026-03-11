//
// Created by dunamis on 11/03/2026.
//

#include <thread>

#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/core/CommunicationManager.h"
#include "smartdrive/transport/PcSerialTransport.h"
#include "smartdrive//generated/IndicatorBoardController.h"
#include <csignal>

volatile bool running = true;

void signalHandler(int) {
    running = false;
}

void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void consoleLogger(LogLevel level, const char* message) {
    const char* levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    std::cout << "[" << levelStr << "] " << message << std::endl;
}

void doSomething(const CommandResponse& response, void* context) {
    switch (response.status) {
        case ProtocolConstants::ResponseStatus::OK:
            LOG(LogLevel::INFO, "[ARDUINO] Command ran successfully!");
            break;
        case ProtocolConstants::ResponseStatus::UNKNOWN_COMMAND_TYPE:
            LOG(LogLevel::ERROR, "Unknown command type");
            break;
        case ProtocolConstants::ResponseStatus::INVALID_PARAMS:
            LOG(LogLevel::ERROR, "Invalid parameters");
            break;
        case ProtocolConstants::ResponseStatus::HARDWARE_BUSY:
            LOG(LogLevel::ERROR, "Hardware is busy");
            break;
        case ProtocolConstants::ResponseStatus::NOT_SUPPORTED:
            LOG(LogLevel::ERROR, "Command not supported");
            break;
        case ProtocolConstants::ResponseStatus::HARDWARE_FAULT:
            LOG(LogLevel::ERROR, "Hardware fault");
            break;
    }
}

int main() {
    Logger::setCallback(consoleLogger);
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
    IndicatorBoardController device(cm);
    // cm.onResponseReceived(doSomething);

    std::thread listenerThread([&cm]() {
        while (running) {
            cm.listen();
        }
    });

    std::thread processingThread([&cm]() {
        while (running) {
            cm.processResponses();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::cout << "\nWaiting for Arduino..." << std::endl;
    wait(6000);
    std::cout << "Arduino is ready." << std::endl;

    for (int i=0; i<2; i++) {
        if (device.oledSetBrightness(13)) {
            std::cout << i+1 << ". [PC] sent OLEDSETBRIGHTNESS command" << std::endl;
        }

        wait(3000);

        if (device.oledSetBrightness(0)) {
            std::cout << i+1 << ". [PC] sent OLEDSETBRIGHTNESS command" << std::endl;
        }

        wait(3000);
    }

    if (device.beep(1000)) {
        std::cout << "[PC] sent BEEP command" << std::endl;
    }


    processingThread.join();
    listenerThread.join();
    return 0;
}