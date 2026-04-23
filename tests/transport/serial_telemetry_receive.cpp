//
// Created by dunamis on 30/03/2026.
//

#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <csignal>

#include "smartdrive/core/CommunicationManager.h"
#include "smartdrive/transport/PcSerialTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/TelemetrySourceIDs.h"

volatile bool running = true;

void wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

int main() {
    const std::string port     = "/dev/ttyUSB0";
    const uint32_t    baudRate = 115200;

    std::cout << "=== SmartDrive Raw Debug Test ===" << std::endl;
    std::cout << "Port: " << port << " @ " << baudRate << " baud" << std::endl;

    // ── Step 1: Open transport ───────────────────────────────────────────────
    std::cout << "\n Opening serial port..." << std::endl;
    PcSerialTransport transport(port, baudRate);
    std::cout << "    Port opened successfully." << std::endl;

    std::cout << "\n\n Waiting for Arduino..." << std::endl;
    wait(4000);

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &transport);
    cm.onTelemetryReceived([](const Telemetry& data, void* ctx) {
       switch (data.sourceID) {
           case TelemetrySource::BOARD_TEMPERATURE: {
               auto voltage = data.unpack<float>();
               std::cout << "Temperature: " << voltage << std::endl;
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