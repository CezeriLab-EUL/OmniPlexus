//
// Raw debug test — bypasses all high level functions
// Manually constructs the OLED_PRINT_STR command and sends it byte by byte
// with full diagnostic logging at every step
//

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>

#include "../../include/smartdrive/transport/serial/PcSerialTransport.h"
#include "smartdrive/protocol/BinaryEncoder.h"
#include "smartdrive/generated/IndicatorBoardController.h"

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

    BinaryEncoder encoder;
    CommunicationManager cm(&encoder, &transport);
    IndicatorBoardController device(cm);

    std::cout << "\n\n Waiting for Arduino..." << std::endl;
    wait(6000);
    std::cout << "Arduino is ready." << std::endl;

    if (device.ledSetBlock(10)) {
        std::cout << "Set LED block" << std::endl;
        wait(3000);
    }else {
        std::cout << "Failed to set LED block" << std::endl;
    }

    if (device.ledSetBlock(0)) {
        std::cout << "Reset LED block" << std::endl;
        wait(3000);
    }else {
        std::cout << "Failed to reset LED block" << std::endl;
    }

    return 0;
}