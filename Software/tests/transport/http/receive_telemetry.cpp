//
// Created by dunamis on 29/04/2026.
//


#include <iostream>
#include <chrono>
#include <filesystem>
#include <thread>

#include "smartdrive/transport/http/PcHttpTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/EspController.h"
#include "smartdrive/generated/TelemetrySourceIDs.h"

volatile bool running = true;

void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void consoleLogger(LogLevel level, const char* message) {
    const char* levelStr;
    switch (level) {
        case LogLevel::OP_DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::OP_INFO: levelStr = "INFO"; break;
        case LogLevel::OP_WARNING: levelStr = "WARN"; break;
        case LogLevel::OP_ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN";
    }
    std::cout << "[" << levelStr << "] " << message << std::endl;
}

int main() {
    Logger::setCallback(consoleLogger);
    const uint16_t port = 4000;

    std::cout << "=== SmartDrive Raw Debug Test ===" << std::endl;
    std::cout << "Server is listening on port: " << port << std::endl;

    // ── Step 1: Open transport ───────────────────────────────────────────────
    std::cout << "\n Creating http server..." << std::endl;
    PcHttpTransport server(port);
    std::cout << "    Server created successfully." << std::endl;

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &server);

    cm.onTelemetryReceived([](const Telemetry& data, void* ctx) {
       switch (data.sourceID) {
           case TelemetrySource::BOARD_TEMPERATURE: {
               auto temperature = data.unpack<float>();
               std::cout << "Temperature: " << temperature << std::endl;
               break;
           }
       }
    });

    std::thread listenerThread([&cm]() {
        while (running) {
            cm.listen();
        }
    });

    listenerThread.join();
    return 0;
}