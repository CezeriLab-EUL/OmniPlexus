#include "smartdrive/transport/wifi/PcWiFiTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/EspController.h"
#include "smartdrive/core/TransportManager.h"
#include "smartdrive/mutex/StdMutex.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace TransportID {
    constexpr uint8_t WIFI = 0;
}

volatile bool running = true;

void wait(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main() {
    const char* host = "192.168.12.136";
    const uint16_t port = 9000;

    std::cout << "Connecting to ESP..." << std::endl;
    PcWiFiTransport transport(host, port);

    if (!transport.isConnected()) {
        std::cout << "Failed to connect" << std::endl;
        return 1;
    }
    std::cout << "Connected!" << std::endl;

    BinaryEncoder encoder;

    TransportManager tm;
    tm.add(&transport, TransportID::WIFI);

    StdMutex sendMutex;
    StdMutex listenMutex;

    CommunicationManager cm(&encoder, &tm, &sendMutex, &listenMutex);
    EspController device(cm);

    auto start = std::chrono::high_resolution_clock::now();
    if (device.turnonBuiltinLed()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "LED ON (took " << ms << "ms)" << std::endl;
        wait(3000);
    } else {
        std::cout << "Failed to turn LED ON" << std::endl;
    }

    start = std::chrono::high_resolution_clock::now();
    if (device.turnoffBuiltinLed()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        std::cout << "LED OFF (took " << ms << "ms)" << std::endl;
    } else {
        std::cout << "Failed to turn LED OFF" << std::endl;
    }

    std::thread listenerThread([&cm]() {
        while (running) {
            cm.listen();
        }
    });

    listenerThread.join();

    return 0;
}