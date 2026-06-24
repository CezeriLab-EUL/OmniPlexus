//
// Created by dunamis on 01/05/2026.
//

#ifndef SMARTDRIVE_OPXSESSION_H
#define SMARTDRIVE_OPXSESSION_H

#ifndef EMBEDDED_BUILD

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "opx/core/CommunicationManager.h"
#include "opx/core/DeviceRegistry.h"
#include "opx/core/PlatformClock.h"
#include "opx/core/TransportManager.h"
#include "opx/interfaces/ITransport.h"
#include "opx/mutex/StdMutex.h"
#include "opx/protocol/BinaryEncoder.h"
#include "opx/registry/CommandRegistry.h"
#include "opx/types/ProtocolTypes.h"
#include "opx/utils/Logger.h"

enum class OpxTransportID : uint8_t { WIFI = 0, SERIAL = 1, HTTP = 2 };

static constexpr uint8_t OPX_MAX_TRANSPORTS = 3;

class OpxSession {
public:
  using TelemetryHandler = std::function<void(const Telemetry &telemetry,
                                              uint8_t sourceTransportID)>;
  using CommandHandler = std::function<void(const Command &cmd, uint8_t seqNum,
                                            uint8_t sourceTransportID)>;
  using ResponseHandler = std::function<void(const CommandResponse &response,
                                             uint8_t sourceTransportID)>;
  using SettingHandler = std::function<void(const SettingsData &setting,
                                            uint8_t sourceTransportID)>;

  OpxSession();

  ~OpxSession();

  OpxSession(const OpxSession &) = delete;

  OpxSession &operator=(const OpxSession &) = delete;

  OpxSession(OpxSession &&) = delete;

  OpxSession &operator=(OpxSession &&) = delete;

  bool connectWiFi(const char *host, uint16_t port,
                   uint8_t maxReconnectAttempts = 5,
                   uint32_t reconnectDelayMs = 2000);

  bool connectSerial(const char *port, uint32_t baudRate);

  bool connectHttp(const char *host, uint16_t port);

  void disconnect(OpxTransportID id);

  void disconnectAll();

  bool isConnected(OpxTransportID id) const;

  bool isAnyConnected() const;

  void onTelemetry(TelemetryHandler handler);

  void onCommand(CommandHandler handler);

  void onCommandResponse(ResponseHandler handler);

  void onSetting(SettingHandler handler);

  void discover();
  void onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb,
                         void *context = nullptr);
  void onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb,
                            void *context = nullptr);
  bool isDeviceConnected(uint8_t typeShift) const;
  uint8_t transportIDFor(uint8_t typeShift) const;

  void setHeartbeatInterval(uint32_t intervalMs);
  void setDeviceTimeout(uint32_t timeoutMs);

  bool dispatch(const Command &cmd,
                uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT) {
    if (!cm.has_value())
      return false;
    return cm->dispatch(cmd, transportID);
  }

  bool getAllSettings(
      uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT) {
    Command cmd;
    cmd.commandType = ProtocolConstants::GET_ALL_SETTINGS_COMMAND;
    return dispatch(cmd, transportID);
  }

  template <typename TController> TController &getDevice();

  CommandRegistry &registry();

private:
  struct TransportSlot {
    ITransport *transport = nullptr;
    OpxTransportID id;
    bool active = false;
  };

  bool slotOccupied(OpxTransportID id) const;

  TransportSlot *findSlot(OpxTransportID id);

  const TransportSlot *findSlot(OpxTransportID id) const;

  void ensureCommunicationManager();

  bool addTransport(ITransport *transport, OpxTransportID id);

  void removeTransport(OpxTransportID id);

  void rewireHandlers();

  void startThreads();

  void stopThreads();

  static void telemetryBridge(const Telemetry &telemetry,
                              uint8_t sourceTransportID, void *context);

  static void commandBridge(const Command &cmd, const uint8_t &seqNum,
                            uint8_t sourceTransportID, void *context);

  static void responseBridge(const CommandResponse &response,
                             uint8_t sourceTransportID, void *context);

  static void settingBridge(const SettingsData &setting,
                            uint8_t sourceTransportID, void *context);

  BinaryEncoder encoder;
  StdMutex sendMutex;
  StdMutex listenMutex;
  TransportManager tm;

  std::optional<CommunicationManager> cm;
  TransportSlot slots[OPX_MAX_TRANSPORTS];
  uint8_t activeSlots = 0;

  TelemetryHandler telemetryHandler;
  CommandHandler commandHandler;
  ResponseHandler responseHandler;
  SettingHandler settingHandler;

  DeviceRegistry deviceRegistry;
  CommandRegistry reg;

  using ControllerDeleter = void (*)(void *);
  std::unordered_map<uint16_t, std::unique_ptr<void, ControllerDeleter>>
      controllerMap;

  std::atomic<bool> running{false};
  std::thread listenerThread;
  std::thread processingThread;

  PlatformClock clock;
  uint32_t heartbeatIntervalMs = 1000;
  uint32_t lastHeartbeatSentMs = 0;
};

template <typename TController> TController &OpxSession::getDevice() {
  if (!cm.has_value()) {
    throw std::runtime_error(
        "OpxSession::getDevice() called before any connect*(). "
        "Call connectWiFi(), connectSerial(), or connectHttp() first.");
  }

  constexpr uint16_t id = TController::TYPE_ID;

  auto it = controllerMap.find(id);
  if (it != controllerMap.end()) {
    return *static_cast<TController *>(it->second.get());
  }

  TController *ptr = new TController(*cm);
  controllerMap.emplace(
      id, std::unique_ptr<void, ControllerDeleter>(
              ptr, [](void *p) { delete static_cast<TController *>(p); }));

  return *ptr;
}

#endif
#endif // SMARTDRIVE_OPXSESSION_H
