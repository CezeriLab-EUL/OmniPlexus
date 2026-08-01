#include "autogen/pc/devices/PcApp/PcAppRegister.h"
#include "autogen/shared/CommandTypes.h"
#include "autogen/shared/TelemetrySourceIDs.h"
#include "opx/pc/core/OpxSession.h"
#include "opx/shared/types/ProtocolTypes.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <thread>

std::atomic<bool> running{true};

void handleSigint(int) { running = false; }

int main() {
  std::signal(SIGINT, handleSigint);

  OpxSession session;

  registerPcApp(session);

  session.onDeviceConnected([](uint8_t typeShift, uint8_t transportID, void *) {
    std::cout << "[pc] discovered peer typeShift=" << (int)typeShift << "\n";
  });

  session.onDeviceDisconnected([](uint8_t typeShift, uint8_t transportID,
                                  void *) {
    std::cout << "[pc] peer typeShift=" << (int)typeShift << " disconnected\n";
  });

  session.onCommand(
      [](const Command &cmd, uint8_t seqNum, uint8_t transportID) {
        switch (cmd.commandType) {
        case PcAppCommandType::PING: {
          std::cout << "Got ping command from deneyap" << std::endl;
          break;
        }
        }
      });

  session.setDeviceTimeout(10000);

  if (!session.connectWiFi("192.168.0.110", 9000)) {
    std::cerr << "Failed to connect to Deneyap Kart\n";
    return 1;
  }

  std::cout << "Connected. Press Ctrl+C to stop.\n";

  float voltage = 3.7f;
  auto lastUpdate = std::chrono::steady_clock::now();

  while (running) {
    auto now = std::chrono::steady_clock::now();
    if (now - lastUpdate >= std::chrono::seconds(1)) {
      voltage += 0.01f;
      ValueSource v = voltage;
      session.updateTelemetry(
          TelemetrySource::PcAppTelemetrySource::BATTERY_VOLTAGE, v);
      lastUpdate = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "\nShutting down...\n";
  return 0;
}