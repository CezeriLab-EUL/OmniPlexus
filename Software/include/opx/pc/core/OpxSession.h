//
// Created by dunamis on 01/05/2026.
//

#pragma once

#include "opx/shared/core/Config.h" // IWYU pragma: keep
#include "opx/shared/core/ValueSource.h"
#include "opx/shared/types/RobotData.h"
#include <cstdint>

#ifndef OPX_TARGET_EMBEDDED
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "opx/pc/registry/CommandRegistry.h"
#include "opx/shared/core/CommunicationManager.h"
#include "opx/shared/core/DeviceRegistry.h"
#include "opx/shared/core/PlatformClock.h"
#include "opx/shared/core/SettingsManager.h"
#include "opx/shared/core/TelemetryManager.h"
#include "opx/shared/core/TransportManager.h"
#include "opx/shared/core/TriggerConfig.h"
#include "opx/shared/interfaces/ITransport.h"
#include "opx/shared/mutex/StdMutex.h"
#include "opx/shared/protocol/BinaryEncoder.h"
#include "opx/shared/types/ProtocolTypes.h"

enum class OpxTransportCategory : uint8_t {
  OPX_WIFI = 0,
  OPX_SERIAL = 1,
  OPX_HTTP = 2
};

// OPX_SESSION_TRANSPORT_ID_STRIDE = instance slots reserved per category
// (see the identical note in OpxDevice.h — same reasoning, no CDnC
// low-ID concern on the PC side, so this starts at 0 instead of 0x30).
static constexpr uint8_t OPX_SESSION_TRANSPORT_ID_STRIDE = 8;
constexpr uint8_t opxComposeSessionTransportID(OpxTransportCategory category,
                                               uint8_t instance) {
  return static_cast<uint8_t>(category) * OPX_SESSION_TRANSPORT_ID_STRIDE +
         instance;
}
class OpxSession {
public:
  // ── Handler type aliases ───────────────────────────────────────────────────
  using TelemetryHandler = std::function<void(const Telemetry &telemetry,
                                              uint8_t sourceTransportID)>;
  using CommandHandler = std::function<void(const Command &cmd, uint8_t seqNum,
                                            uint8_t sourceTransportID)>;
  using ResponseHandler = std::function<void(const CommandResponse &response,
                                             uint8_t sourceTransportID)>;
  using SettingHandler = std::function<void(const SettingsData &setting,
                                            uint8_t sourceTransportID)>;

  using ProtocolCommandHook =
      std::function<void(const Command &cmd, uint8_t sourceTransportID)>;

  // ── Construction / Destruction ─────────────────────────────────────────────
  OpxSession();

  ~OpxSession();

  OpxSession(const OpxSession &) = delete;

  OpxSession &operator=(const OpxSession &) = delete;

  OpxSession(OpxSession &&) = delete;

  OpxSession &operator=(OpxSession &&) = delete;

  // ── Transport Setup
  // ─────────────────────────────────────────────────────────
  bool connectWiFi(const char *host, uint16_t port,
                   uint8_t maxReconnectAttempts = 5,
                   uint32_t reconnectDelayMs = 2000, uint8_t instance = 0);

  bool connectSerial(const char *port, uint32_t baudRate, uint8_t instance = 0);

  bool connectHttp(const char *host, uint16_t port, uint8_t instance = 0);

  bool beginWiFi(uint16_t port, uint8_t instance = 0);

  bool beginHttpServer(uint16_t port, uint8_t instance = 0);

  // ── Transport Teardown
  // ──────────────────────────────────────────────────────
  void disconnect(OpxTransportCategory category, uint8_t instance = 0);

  void disconnectAll();

  // ── Connection Status
  // ───────────────────────────────────────────────────────
  bool isConnected(OpxTransportCategory category, uint8_t instance = 0) const;

  bool isAnyConnected() const;

  // ── Event Handlers
  // ───────────────────────────────────────────────────────────
  void onTelemetry(TelemetryHandler handler);

  void onCommand(CommandHandler handler);

  void onResponse(ResponseHandler handler);

  void onSetting(SettingHandler handler);

  void onDiscover(ProtocolCommandHook hook);

  void onAnnounce(ProtocolCommandHook hook);

  void onHeartbeat(ProtocolCommandHook hook);

  void onHeartbeatAck(ProtocolCommandHook hook);

  // ── Discovery
  // ────────────────────────────────────────────────────────────────
  void discover();
  void onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb,
                         void *context = nullptr);
  void onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb,
                            void *context = nullptr);
  bool isDeviceConnected(uint8_t typeShift) const;
  uint8_t transportIDFor(uint8_t typeShift) const;
  void setTypeShift(uint8_t typeShift);
  void announce();

  // ── Heartbeat
  // ────────────────────────────────────────────────────────────────
  void setHeartbeatInterval(uint32_t intervalMs);
  void setDeviceTimeout(uint32_t timeoutMs);
  void setAnnounceInterval(uint32_t intervalMs);

  // ── Sending
  // ──────────────────────────────────────────────────────────────────
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

  // ── Telemetry Management (session as identity)
  // ──────────────────────────────
  bool registerTelemetry(uint16_t sourceID, TriggerConfig trigger);
  bool updateTelemetry(uint16_t sourceID, const ValueSource &value);
  bool sendTelemetryNow(uint16_t sourceID);
  bool setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger);
  bool enableTelemetry(uint16_t sourceID);
  bool disableTelemetry(uint16_t sourceID);
  bool unregisterTelemetry(uint16_t sourceID);

  // ── Settings Management (session as identity)
  // ────────────────────────────────────────────────────────────
  bool registerSetting(uint16_t settingID, ValueType type);
  bool updateSetting(uint16_t settingID, const ValueSource &value,
                     bool broadcast = false);
  bool attachSettingCallback(uint16_t settingID,
                             SettingsManager::SettingChangedCallback cb,
                             void *context = nullptr);
  void onAnySettingChanged(SettingsManager::SettingChangedCallback cb,
                           void *context = nullptr);
  void broadcastAllSettings();
  void broadcastOneSetting(uint16_t settingID);
  const SettingsData *getSetting(uint16_t settingID) const;

  // ── Device Access
  // ────────────────────────────────────────────────────────────
  template <typename TController> TController &getDevice();

  CommandRegistry &registry();

private:
  // ── Nested types
  // ─────────────────────────────────────────────────────────────
  struct TransportSlot {
    ITransport *transport = nullptr;
    uint8_t id;
    bool active = false;
  };

  // ── Core protocol state
  // ───────────────────────────────────────────────────────
  BinaryEncoder encoder;
  StdMutex sendMutex;
  StdMutex listenMutex;
  TransportManager tm;
  std::optional<CommunicationManager> cm;
  SettingsManager *settingsManager = nullptr;
  TelemetryManager *telemetryManager = nullptr;

  void ensureCommunicationManager();
  void ensureSettingsManager();
  void ensureTelemetryManager();

  // ── Transport slots
  // ───────────────────────────────────────────────────────────
  TransportSlot slots[MAX_TRANSPORTS];
  uint8_t activeSlots = 0;

  bool slotOccupied(uint8_t id) const;

  TransportSlot *findSlot(uint8_t id);

  const TransportSlot *findSlot(uint8_t id) const;

  bool addTransport(ITransport *transport, uint8_t id);

  void removeTransport(uint8_t id);

  // ── User callbacks
  // ────────────────────────────────────────────────────────────
  TelemetryHandler telemetryHandler;
  CommandHandler commandHandler;
  ResponseHandler responseHandler;
  SettingHandler settingHandler;

  void rewireHandlers();

  // ── Registries
  // ───────────────────────────────────────────────────────────────
  DeviceRegistry deviceRegistry;
  CommandRegistry reg;

  using ControllerDeleter = void (*)(void *);
  std::unordered_map<uint16_t, std::unique_ptr<void, ControllerDeleter>>
      controllerMap;

  // ── Background threads
  // ────────────────────────────────────────────────────────
  std::atomic<bool> running{false};
  std::thread listenerThread;
  std::thread processingThread;

  void startThreads();

  void stopThreads();

  // ── Heartbeat state
  // ───────────────────────────────────────────────────────────
  PlatformClock clock;
  uint32_t heartbeatIntervalMs = 1000;
  uint32_t lastHeartbeatSentMs = 0;
  uint8_t ownTypeShift = 0xFF;
  // Periodic re-announce: self-heals a missed/lost initial announce.
  uint32_t announceIntervalMs = 15000;
  uint32_t lastAnnounceSentMs = 0;

  ProtocolCommandHook discoverHook;
  ProtocolCommandHook announceHook;
  ProtocolCommandHook heartbeatHook;
  ProtocolCommandHook heartbeatAckHook;

  // ── Static protocol bridges (CommunicationManager callbacks) ──────────────
  static void telemetryBridge(const Telemetry &telemetry,
                              uint8_t sourceTransportID, void *context);

  static void commandBridge(const Command &cmd, const uint8_t &seqNum,
                            uint8_t sourceTransportID, void *context);

  static void responseBridge(const CommandResponse &response,
                             uint8_t sourceTransportID, void *context);

  static void settingBridge(const SettingsData &setting,
                            uint8_t sourceTransportID, void *context);
};

// ── getDevice<T>() template definition ───────────────────────────────────────
// Controllers are lazily constructed and cached per session; the map owns
// them for the session's lifetime so callers can hold references safely.
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
