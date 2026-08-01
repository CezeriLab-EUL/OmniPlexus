//
// OmniPlexus (opx) - Embedded Device Facade
//

#pragma once

#include "opx/shared/core/Config.h" // IWYU pragma: keep

#ifdef OPX_FRAMEWORK_ARDUINO
#include "opx/shared/core/CommunicationManager.h"
#include "opx/shared/core/DeviceRegistry.h"
#include "opx/shared/core/PlatformClock.h"
#include "opx/shared/core/SettingsManager.h"
#include "opx/shared/core/TelemetryManager.h"
#include "opx/shared/core/TransportManager.h"
#include "opx/shared/core/TriggerConfig.h"
#include "opx/shared/core/platform.h"
#include "opx/shared/interfaces/IConnectable.h"
#include "opx/shared/interfaces/IPlatformClock.h"
#include "opx/shared/interfaces/ITransport.h"
#include "opx/shared/protocol/BinaryEncoder.h"
#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/utils/Logger.h"

#if OPX_TARGET_ESP32
#include "opx/embedded/transport/http/EspHttpTransport.h"
#include "opx/embedded/transport/wifi/EspWiFiTransport.h"
#include "opx/shared/mutex/FreeRtosMutex.h"
#endif

#include "opx/embedded/transport/serial/ArduinoSerialTransport.h"
#include "opx/shared/mutex/NullMutex.h"

#if OPX_CDNC_MASTER
#include "opx/embedded/transport/cdnc/CDnC.h"
#include "opx/embedded/transport/cdnc/CDnCManager.h"
#include "opx/embedded/transport/cdnc/CDnCTransport.h"
#endif

#if OPX_CDNC_SLAVE
#include "opx/embedded/transport/cdnc/CDnCSlaveManager.h"
#include "opx/embedded/transport/cdnc/CDnCSlaveTransport.h"
#endif

// IDs for the transports OpxDevice can own directly (see TransportSlot).
// CDnC and other raw/manually-added transports are identified by their own
// TransportManager-assigned IDs and don't appear here.
enum class OpxDeviceTransportID : uint8_t {
  OPX_SERIAL = 0x30,
  OPX_WIFI = 0x31,
  OPX_HTTP = 0x32,
};

class OpxDevice {
public:
  // ── Handler type aliases ───────────────────────────────────────────────────
  using CommandHandler = void (*)(const Command &, const uint8_t &,
                                  uint8_t sourceTransportID, void *context);
  using ResponseHandler = void (*)(const CommandResponse &,
                                   uint8_t sourceTransportID, void *context);
  using TelemetryHandler = void (*)(const Telemetry &,
                                    uint8_t sourceTransportID, void *context);
  using SettingHandler = void (*)(const SettingsData &,
                                  uint8_t sourceTransportID, void *context);
  using ConnectionLostHandler = void (*)();

  // ── Construction / Destruction ─────────────────────────────────────────────
  OpxDevice();
  ~OpxDevice();
  OpxDevice(const OpxDevice &) = delete;
  OpxDevice &operator=(const OpxDevice &) = delete;

  // ── Transport Setup ────────────────────────────────────────────────────────
  template <typename SerialType>
  bool beginSerial(SerialType &serial, uint32_t baud);

  template <typename SerialType>
  bool connectSerial(SerialType &serial, uint32_t baud) {
    return beginSerial(serial, baud);
  }

#if OPX_TARGET_ESP32
  bool beginWiFi(uint16_t port, uint32_t stackSize = 4096);
  bool beginHttpServer(uint16_t port, uint32_t stackSize = 4096);
  bool beginHttpClient(const char *host, uint16_t port);
  bool connectWiFi(const char *host, uint16_t port,
                   uint8_t maxReconnectAttempts = 5,
                   uint32_t reconnectDelayMs = 2000, uint32_t stackSize = 4096);
  bool connectHttp(const char *host, uint16_t port);
#endif

  bool addRawTransport(ITransport *transport, uint8_t id);

  // ── Transport Teardown ─────────────────────────────────────────────────────
  void end(OpxDeviceTransportID id);
  void endAll();

  // ── Main Loop
  // ───────────────────────────────────────────────────────────────
  void update();

  // ── Sending
  // ─────────────────────────────────────────────────────────────────
  bool
  sendCommand(const Command &cmd,
              uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT);
  bool sendResponse(const CommandResponse &response);
  bool sendResponse(uint8_t seqNum, uint16_t commandType,
                    ProtocolConstants::ResponseStatus status);
  bool sendTelemetry(const Telemetry &telemetry);

  // ── Event Handlers
  // ──────────────────────────────────────────────────────────
  void onCommand(CommandHandler handler, void *context = nullptr);
  void onResponse(ResponseHandler handler, void *context = nullptr);
  void onTelemetry(TelemetryHandler handler, void *context = nullptr);
  void onSetting(SettingHandler handler, void *context = nullptr);
  void onConnectionLost(ConnectionLostHandler callback);

  // ── Discovery
  // ────────────────────────────────────────────────────────────────
  void setTypeShift(uint8_t typeShift);
  void announce();
  void discover();
  void onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb,
                         void *context = nullptr);
  void onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb,
                            void *context = nullptr);
  bool isDeviceConnected(uint8_t typeShift) const;
  uint8_t transportIDFor(uint8_t typeShift) const;

  // ── Protocol-level command hooks ──────────────────────────────────────────
  // Called AFTER the library's own default handling for each protocol command.
  // Use on bridge nodes (e.g. STM32 routing CDnC ↔ Serial) to intercept
  // DISCOVER/ANNOUNCE/HEARTBEAT without blocking the library's protocol logic.
  using ProtocolCommandHook = void (*)(const Command &, uint8_t srcTransportID,
                                       void *ctx);

  void onDiscover(ProtocolCommandHook hook, void *ctx = nullptr);
  void onAnnounce(ProtocolCommandHook hook, void *ctx = nullptr);
  void onHeartbeat(ProtocolCommandHook hook, void *ctx = nullptr);
  void onHeartbeatAck(ProtocolCommandHook hook, void *ctx = nullptr);

  // ── Heartbeat
  // ────────────────────────────────────────────────────────────────
  void setHeartbeatTimeout(uint32_t timeoutMs);
  void setPeerHeartbeatInterval(uint32_t intervalMs);
  void setAnnounceInterval(uint32_t intervalMs);

  // ── Telemetry management ──────────────────────────────────────────────────
  bool registerTelemetry(uint16_t sourceID, TriggerConfig trigger);
  bool updateTelemetry(uint16_t sourceID, const ValueSource &value);
  bool sendTelemetryNow(uint16_t sourceID);
  bool setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger);
  bool enableTelemetry(uint16_t sourceID);
  bool disableTelemetry(uint16_t sourceID);
  bool unregisterTelemetry(uint16_t sourceID);

  // ── Settings management ───────────────────────────────────────────────────
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

  // ── Forwarding
  // ───────────────────────────────────────────────────────────────
  bool forwardBetween(uint8_t transportA, uint8_t transportB);

#if OPX_CDNC_MASTER
  // ── CDnC master
  // ──────────────────────────────────────────────────────────────
  using CdncSlaveCallback = void (*)(uint8_t slaveIndex, void *context);
  void onCdncSlaveConnected(CdncSlaveCallback cb, void *context = nullptr);
  void onCdncSlaveDisconnected(CdncSlaveCallback cb, void *context = nullptr);

  bool beginCDnC();
  uint16_t exchangeCDnC();
  void endCDnC();

  bool cdncSlaveAlive(uint8_t slave);
  uint16_t cdncAliveMask();
  cdnc_slave_state_t cdncSlaveState(uint8_t slave);

  bool cdncSend(uint8_t slave, uint8_t byte);
  bool cdncRecv(uint8_t slave, uint8_t *out);
  uint8_t cdncAvailable(uint8_t slave);
#endif

#if OPX_CDNC_SLAVE
  // ── CDnC slave
  // ───────────────────────────────────────────────────────────────
  bool beginCDnC(uint8_t dataPin, uint8_t clkPin);
  void endCDnCSlave();
#endif

  // ── Escape hatch
  // ─────────────────────────────────────────────────────────────
  CommunicationManager *comms();
  TransportManager *transportManager() { return &tm; }

  // ── Device Access ────────────────────────────────────────────────────────
  // Unlike OpxSession::getDevice<T>(), this returns a fresh, cheap-to-construct
  // value each call rather than a cached reference — generated Controller
  // classes are stateless wrappers around CommunicationManager&, so there's
  // nothing worth caching, and avoiding a heap-backed cache keeps this safe
  // on constrained targets (AVR, etc.).
  template <typename TController> TController getDevice() {
    return TController(*comms());
  }

private:
  // ── Nested types
  // ─────────────────────────────────────────────────────────────
  struct TransportSlot {
    ITransport *transport = nullptr;
    IConnectable *connectable = nullptr;
    OpxDeviceTransportID id;
    bool active = false;
  };

  struct ForwardingPair {
    uint8_t transportA = 0;
    uint8_t transportB = 0;
    bool active = false;
  };

  // ── Core protocol state
  // ───────────────────────────────────────────────────────
  BinaryEncoder encoder;
  PlatformClock clock;
  TransportManager tm;
  DeviceRegistry deviceRegistry;
  uint8_t ownTypeShift = 0xFF;

#if OPX_TARGET_ESP32
  FreeRtosMutex sendMutex;
  FreeRtosMutex listenMutex;
#else
  NullMutex sendMutex;
  NullMutex listenMutex;
#endif

  // Lazily created on first use so unused features (telemetry, settings) cost
  // nothing on constrained targets.
  CommunicationManager *cm = nullptr;
  TelemetryManager *telemetryManager = nullptr;
  SettingsManager *settingsManager = nullptr;

  void ensureCommunicationManager();
  void ensureTelemetryManager();
  void ensureSettingsManager();
  void rewireHandlers();

  // ── Transport slots
  // ───────────────────────────────────────────────────────────
  static constexpr uint8_t MAX_DEVICE_TRANSPORTS = 3;
  TransportSlot slots[MAX_DEVICE_TRANSPORTS];
  uint8_t activeSlotCount = 0;

  TransportSlot *findSlot(OpxDeviceTransportID id);
  bool slotOccupied(OpxDeviceTransportID id) const;
  bool hasAnyConnectedPeer() const;
  bool addTransport(ITransport *transport, OpxDeviceTransportID id,
                    IConnectable *connectable = nullptr);
  void removeTransport(OpxDeviceTransportID id);

  // ── Forwarding
  // ───────────────────────────────────────────────────────────────
  ForwardingPair forwardingPairs[MAX_FORWARDING_PAIRS];
  void handleForwarding(const TaggedFrame &frame);
  static uint8_t extractTypeShift(const RawData &frame);
  static void forwardBridge(const TaggedFrame &frame, void *context);

  // ── Heartbeat state
  // ───────────────────────────────────────────────────────────
  uint32_t heartbeatTimeoutMs = 3000;
  uint32_t lastHeartbeatMs = 0;
  bool heartbeatReceived = false;
  bool connectionLost = false;
  // this device sending heartbeats to its own peers
  uint32_t peerHeartbeatIntervalMs = 1000;
  uint32_t lastPeerHeartbeatSentMs = 0;
  // Periodic re-announce: self-heals a missed/lost initial announce.
  // Deliberately much longer than peerHeartbeatIntervalMs — announce is
  // heavier and doesn't need heartbeat-frequency repetition.
  uint32_t announceIntervalMs = 15000;
  uint32_t lastAnnounceSentMs = 0;

  // ── User callbacks
  // ────────────────────────────────────────────────────────────
  CommandHandler commandHandler = nullptr;
  void *commandHandlerContext = nullptr;
  ResponseHandler responseHandler = nullptr;
  void *responseHandlerContext = nullptr;
  TelemetryHandler telemetryHandler = nullptr;
  void *telemetryHandlerContext = nullptr;
  SettingHandler settingHandler = nullptr;
  void *settingHandlerContext = nullptr;
  ConnectionLostHandler connectionLostCallback = nullptr;

  // Protocol-level command hooks
  ProtocolCommandHook _discoverHook = nullptr;
  void *_discoverHookCtx = nullptr;
  ProtocolCommandHook _announceHook = nullptr;
  void *_announceHookCtx = nullptr;
  ProtocolCommandHook _heartbeatHook = nullptr;
  void *_heartbeatHookCtx = nullptr;
  ProtocolCommandHook _heartbeatAckHook = nullptr;
  void *_heartbeatAckHookCtx = nullptr;

  // ── Static protocol bridges (CommunicationManager callbacks) ──────────────
  static void commandBridge(const Command &cmd, const uint8_t &seqNum,
                            uint8_t sourceTransportID, void *context);
  static void responseBridge(const CommandResponse &response,
                             uint8_t sourceTransportID, void *context);
  static void telemetryBridge(const Telemetry &telemetry,
                              uint8_t sourceTransportID, void *context);
  static void settingBridge(const SettingsData &setting,
                            uint8_t sourceTransportID, void *context);

#if OPX_TARGET_ESP32
  // ── ESP32 listen task
  // ─────────────────────────────────────────────────────────
  TaskHandle_t listenTaskHandle = nullptr;
  volatile bool listenTaskShouldStop = false;
  SemaphoreHandle_t listenTaskDoneSem = nullptr;
  friend void opxListenTask(void *param);
  void ensureListenTaskStarted(uint32_t stackSize = 4096);
  void ensureListenedTo();
  void stopListenTask();
#endif

#if OPX_CDNC_MASTER
  // ── CDnC master state
  // ─────────────────────────────────────────────────────────
  CDnCManager *cdncManager = nullptr;
  bool cdncActive = false;
  uint16_t _cdncPrevAliveMask = 0;

  CdncSlaveCallback _cdncSlaveConnectedCb = nullptr;
  void *_cdncSlaveConnectedCtx = nullptr;
  CdncSlaveCallback _cdncSlaveDisconnectedCb = nullptr;
  void *_cdncSlaveDisconnectedCtx = nullptr;
#endif

#if OPX_CDNC_SLAVE
  // ── CDnC slave state
  // ──────────────────────────────────────────────────────────
  CDnCSlaveManager *_cdncSlaveManager = nullptr;
  bool _cdncSlaveActive = false;
#endif
};

// ── beginSerial() template definition ────────────────────────────────────────
template <typename SerialType>
bool OpxDevice::beginSerial(SerialType &serial, uint32_t baud) {
  if (slotOccupied(OpxDeviceTransportID::OPX_SERIAL)) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: SERIAL slot already occupied. Call end(SERIAL) first.");
    return false;
  }
  auto *transport = new ArduinoSerialTransport<SerialType>(serial, baud);
  transport->begin();
  return addTransport(transport, OpxDeviceTransportID::OPX_SERIAL);
}

// ── Protocol hook setters (inline — see note below) ──────────────────────────
// These, and the CDnC blocks that follow, must live in the header so the
// sketch's #define OPX_CDNC_MASTER / OPX_CDNC_SLAVE is visible at compile
// time. The Arduino build system compiles library .cpp files separately and
// does not see defines from the sketch.
inline void OpxDevice::onDiscover(ProtocolCommandHook h, void *c) {
  _discoverHook = h;
  _discoverHookCtx = c;
}
inline void OpxDevice::onAnnounce(ProtocolCommandHook h, void *c) {
  _announceHook = h;
  _announceHookCtx = c;
}
inline void OpxDevice::onHeartbeat(ProtocolCommandHook h, void *c) {
  _heartbeatHook = h;
  _heartbeatHookCtx = c;
}
inline void OpxDevice::onHeartbeatAck(ProtocolCommandHook h, void *c) {
  _heartbeatAckHook = h;
  _heartbeatAckHookCtx = c;
}

// ── OPX_CDNC_MASTER inline definitions ───────────────────────────────────────
#if OPX_CDNC_MASTER

inline bool OpxDevice::beginCDnC() {
  if (cdncActive) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: CDnC already active.");
    return false;
  }
  ensureCommunicationManager();
  cdnc_init();
  cdncManager = new CDnCManager();
  if (!cdncManager->init(&tm)) {
    LOG(LogLevel::OP_ERROR, "OpxDevice: CDnCManager failed to initialize.");
    delete cdncManager;
    cdncManager = nullptr;
    return false;
  }
  _cdncPrevAliveMask = 0;
  cdncActive = true;
  return true;
}

inline void OpxDevice::endCDnC() {
  if (!cdncActive)
    return;
  for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++)
    tm.remove(i);
  tm.remove(CDNC_TRANSPORT_ID_BROADCAST);
  delete cdncManager;
  cdncManager = nullptr;
  cdncActive = false;
}

inline uint16_t OpxDevice::exchangeCDnC() {
  if (!cdncActive)
    return 0;
  uint16_t valid = cdnc_exchange();
  delayMicroseconds(CDNC_GAP_US);
  uint16_t aliveMask = cdnc_alive_mask();
  uint16_t changed = aliveMask ^ _cdncPrevAliveMask;
  if (changed) {
    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
      if (!((changed >> s) & 1))
        continue;
      bool nowAlive = (aliveMask >> s) & 1;
      if (nowAlive) {
        if (_cdncSlaveConnectedCb)
          _cdncSlaveConnectedCb(s, _cdncSlaveConnectedCtx);
      } else {
        if (_cdncSlaveDisconnectedCb)
          _cdncSlaveDisconnectedCb(s, _cdncSlaveDisconnectedCtx);
        deviceRegistry.removeByTransport(s);
      }
    }
    _cdncPrevAliveMask = aliveMask;
  }
  return valid;
}

inline bool OpxDevice::cdncSlaveAlive(uint8_t s) { return cdnc_slave_alive(s); }
inline uint16_t OpxDevice::cdncAliveMask() { return cdnc_alive_mask(); }
inline cdnc_slave_state_t OpxDevice::cdncSlaveState(uint8_t s) {
  return cdnc_slave_state_get(s);
}
inline bool OpxDevice::cdncSend(uint8_t s, uint8_t b) {
  return cdnc_send_byte(s, b);
}
inline bool OpxDevice::cdncRecv(uint8_t s, uint8_t *o) {
  return cdnc_recv_byte(s, o);
}
inline uint8_t OpxDevice::cdncAvailable(uint8_t s) {
  return cdnc_rx_available(s);
}

inline void OpxDevice::onCdncSlaveConnected(CdncSlaveCallback cb, void *ctx) {
  _cdncSlaveConnectedCb = cb;
  _cdncSlaveConnectedCtx = ctx;
}
inline void OpxDevice::onCdncSlaveDisconnected(CdncSlaveCallback cb,
                                               void *ctx) {
  _cdncSlaveDisconnectedCb = cb;
  _cdncSlaveDisconnectedCtx = ctx;
}

#endif // OPX_CDNC_MASTER

// ── OPX_CDNC_SLAVE inline definitions ────────────────────────────────────────
#if OPX_CDNC_SLAVE

inline bool OpxDevice::beginCDnC(uint8_t dataPin, uint8_t clkPin) {
  if (_cdncSlaveActive) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: CDnC slave already active.");
    return false;
  }
  ensureCommunicationManager();
  _cdncSlaveManager = new CDnCSlaveManager(dataPin, clkPin);
  if (!_cdncSlaveManager->init(&tm)) {
    LOG(LogLevel::OP_ERROR,
        "OpxDevice: CDnCSlaveManager failed to initialize.");
    delete _cdncSlaveManager;
    _cdncSlaveManager = nullptr;
    return false;
  }
  _cdncSlaveActive = true;
  return true;
}

inline void OpxDevice::endCDnCSlave() {
  if (!_cdncSlaveActive)
    return;
  tm.remove(CDNC_SLAVE_TRANSPORT_ID);
  delete _cdncSlaveManager;
  _cdncSlaveManager = nullptr;
  _cdncSlaveActive = false;
}

#endif // OPX_CDNC_SLAVE

#endif // OPX_FRAMEWORK_ARDUINO
