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

#include "smartdrive/transport/PcSerialTransport.h"
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

    // if (device.beep(1000)) {
    //     std::cout << "Beeped" << std::endl;
    // }else {
    //     std::cout << "Failed to beep" << std::endl;
    // }

    // if (device.oledPrintStr(10,20,"Hello")) {
    //     std::cout << "Printed to the screen" << std::endl;
    //     wait(1000);
    // }else {
    //     std::cout << "Failed to print" << std::endl;
    // }

    if (device.ledSetBlock(0x0000)) {
        std::cout << "Set LED block" << std::endl;
    }else {
        std::cout << "Failed to set LED block" << std::endl;
    }

    return 0;
}