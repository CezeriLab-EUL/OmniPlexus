//
// Created by dunamis on 24/04/2026.
//

#include <iostream>
#include <chrono>
#include <filesystem>
#include <thread>

#include "smartdrive/transport/http/PcHttpTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/EspController.h"

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
    const char* host = "192.168.12.82";

    std::cout << "=== SmartDrive Raw Debug Test ===" << std::endl;
    std::cout << "Host: " << host << ":" << port << std::endl;

    // ── Step 1: Open transport ───────────────────────────────────────────────
    std::cout << "\n Creating http client..." << std::endl;
    PcHttpTransport client(host, port);
    std::cout << "    client created successfully." << std::endl;

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &client);
    EspController device(cm);

    std::cout << "\n\n Waiting for ESP..." << std::endl;
    wait(4000);
    std::cout << "ESP is ready." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    if (device.turnonBuiltinLed()) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Led is ON (took " << duration.count() << "ms)" << std::endl;
        wait(3000);
        start = std::chrono::high_resolution_clock::now();
    }else {
        std::cout << "Failed to set LED ON" << std::endl;
    }

    if (device.turnoffBuiltinLed()) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Led is OFF (took " << duration.count() << "ms)" << std::endl;
        wait(3000);
    }else {
        std::cout << "Failed to turn led off" << std::endl;
    }

    return 0;
}