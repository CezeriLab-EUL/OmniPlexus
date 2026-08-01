//
// Created by dunamis on 01/05/2026.
//

#include "opx/pc/core/OpxSession.h"
#include "opx/shared/constants/ProtocolConstants.h"
#include "opx/shared/core/Config.h" // IWYU pragma: keep
#include "opx/shared/core/TriggerConfig.h"
#include "opx/shared/core/ValueSource.h"
#include "opx/shared/utils/Logger.h"
#include <cstdint>

#ifndef OPX_TARGET_EMBEDDED
#include "opx/pc/transport/http/PcHttpTransport.h"
#include "opx/pc/transport/serial/PcSerialTransport.h"
#include "opx/pc/transport/wifi/PcWiFiTransport.h"
#include "opx/shared/interfaces/IConnectable.h"
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

OpxSession::OpxSession() {
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    slots[i].active = false;
    slots[i].transport = nullptr;
  }
  deviceRegistry.setClock(&clock);
}

OpxSession::~OpxSession() { disconnectAll(); }

// ─────────────────────────────────────────────────────────────────────────────
// Transport Setup
// ─────────────────────────────────────────────────────────────────────────────

bool OpxSession::connectWiFi(const char *host, uint16_t port,
                             uint8_t maxReconnectAttempts,
                             uint32_t reconnectDelayMs) {
  if (slotOccupied(OpxTransportID::WIFI)) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: WIFI slot already occupied. Call disconnect(WIFI) first.");
    return false;
  }

  auto *transport =
      new PcWiFiTransport(host, port, maxReconnectAttempts, reconnectDelayMs);
  // PcWiFiTransport attempts connection in its constructor but doesn't throw on
  // failure — it sets an internal flag instead. We check IConnectable here to
  // catch that failure and report it cleanly before the transport enters the
  // slot.
  auto *connectable = dynamic_cast<IConnectable *>(transport);
  if (connectable && !connectable->isConnected()) {
    LOG(LogLevel::OP_ERROR, "OpxSession: WiFi connection failed.");
    delete transport;
    return false;
  }
  return addTransport(transport, OpxTransportID::WIFI);
}

bool OpxSession::connectSerial(const char *port, uint32_t baudRate) {
  if (slotOccupied(OpxTransportID::SERIAL)) {
    LOG(LogLevel::OP_WARNING, "OpxSession: SERIAL slot already occupied. Call "
                              "disconnect(SERIAL) first.");
    return false;
  }

  PcSerialTransport *transport = nullptr;

  // Unlike WiFi and HTTP, PcSerialTransport throws a
  // boost::system::system_error if the port doesn't exist or is already in use.
  // We catch it here to maintain our no-exceptions contract with the frontend.

  try {
    transport = new PcSerialTransport(port, baudRate);
  } catch (const std::exception &e) {
    LOG(LogLevel::OP_ERROR, "OpxSession: Serial connection failed");
    return false;
  }

  return addTransport(transport, OpxTransportID::SERIAL);
}

bool OpxSession::connectHttp(const char *host, uint16_t port) {
  if (slotOccupied(OpxTransportID::HTTP)) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: HTTP slot already occupied. Call disconnect(HTTP) first.");
    return false;
  }

  auto *transport = new PcHttpTransport(host, port);
  return addTransport(transport, OpxTransportID::HTTP);
}

bool OpxSession::beginWiFi(uint16_t port) {
  if (slotOccupied(OpxTransportID::WIFI)) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: WIFI slot already occupied. Call disconnect(WIFI) first.");
    return false;
  }

  auto *transport = new PcWiFiTransport(port);
  return addTransport(transport, OpxTransportID::WIFI);
}

bool OpxSession::beginHttpServer(uint16_t port) {
  if (slotOccupied(OpxTransportID::HTTP)) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: HTTP slot already occupied. Call disconnect(HTTP) first.");
    return false;
  }

  auto *transport = new PcHttpTransport(port);
  return addTransport(transport, OpxTransportID::HTTP);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport Teardown
// ─────────────────────────────────────────────────────────────────────────────

void OpxSession::disconnect(OpxTransportID id) {
  deviceRegistry.removeByTransport(static_cast<uint8_t>(id));
  removeTransport(id);
  // controllerMap intentionally not cleared — controllers survive
  // individual transport disconnections and are reusable on reconnect
}

void OpxSession::disconnectAll() {
  // Threads must be stopped before transports are deleted. The listener thread
  // calls accumulate() on active transports — deleting a transport while the
  // thread is reading from it is undefined behavior.
  stopThreads();
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    if (slots[i].active) {
      tm.remove(static_cast<uint8_t>(slots[i].id));
      delete slots[i].transport;
      slots[i].transport = nullptr;
      slots[i].active = false;
    }
  }
  activeSlots = 0;
  delete settingsManager;
  settingsManager = nullptr;
  delete telemetryManager;
  telemetryManager = nullptr;
  // cm is destroyed after transports so any final frames processed during
  // stopThreads() can still be dispatched. Controllers are cleared after cm
  // because their destructors may reference cm internals.
  cm.reset();
  controllerMap.clear();
  deviceRegistry.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection Status
// ─────────────────────────────────────────────────────────────────────────────

bool OpxSession::isConnected(OpxTransportID id) const {
  const TransportSlot *slot = findSlot(id);
  if (slot == nullptr || !slot->active) {
    return false;
  }
  const auto *connectable = dynamic_cast<const IConnectable *>(slot->transport);
  if (connectable) {
    return connectable->isConnected();
  }
  return true;
}

bool OpxSession::isAnyConnected() const {
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    if (slots[i].active) {
      const auto *connectable =
          dynamic_cast<const IConnectable *>(slots[i].transport);
      if (connectable) {
        if (connectable->isConnected()) {
          return true;
        }
      } else {
        return true;
      }
    }
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event Handlers
// ─────────────────────────────────────────────────────────────────────────────

void OpxSession::onTelemetry(TelemetryHandler handler) {
  telemetryHandler = std::move(handler);
  if (cm.has_value()) {
    cm->onTelemetryReceived(telemetryBridge, this);
  }
}

void OpxSession::onCommand(CommandHandler handler) {
  commandHandler = std::move(handler);
}

void OpxSession::onResponse(ResponseHandler handler) {
  responseHandler = std::move(handler);
  if (cm.has_value()) {
    cm->onResponseReceived(responseBridge, this);
  }
}

void OpxSession::onSetting(SettingHandler handler) {
  settingHandler = std::move(handler);
  if (cm.has_value()) {
    cm->onSettingReceived(settingBridge, this);
  }
}

void OpxSession::onDiscover(ProtocolCommandHook hook) {
  discoverHook = std::move(hook);
}

void OpxSession::onAnnounce(ProtocolCommandHook hook) {
  announceHook = std::move(hook);
}

void OpxSession::onHeartbeat(ProtocolCommandHook hook) {
  heartbeatHook = std::move(hook);
}

void OpxSession::onHeartbeatAck(ProtocolCommandHook hook) {
  heartbeatAckHook = std::move(hook);
}

// ─────────────────────────────────────────────────────────────────────────────
// Discovery
// ─────────────────────────────────────────────────────────────────────────────

void OpxSession::discover() {
  if (!cm.has_value())
    return;
  Command cmd;
  cmd.commandType = ProtocolConstants::DISCOVER_COMMAND;
  cm->dispatchCommandToAll(cmd);
}

void OpxSession::onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb,
                                   void *context) {
  deviceRegistry.onDeviceConnected(cb, context);
}

void OpxSession::onDeviceDisconnected(
    DeviceRegistry::DeviceDisconnectedCallback cb, void *context) {
  deviceRegistry.onDeviceDisconnected(cb, context);
}

bool OpxSession::isDeviceConnected(uint8_t typeShift) const {
  return deviceRegistry.isConnected(typeShift);
}

uint8_t OpxSession::transportIDFor(uint8_t typeShift) const {
  return deviceRegistry.transportIDFor(typeShift);
}

void OpxSession::setTypeShift(uint8_t typeShift) { ownTypeShift = typeShift; }

void OpxSession::announce() {
  if (!cm.has_value()) {
    return;
  }
  if (ownTypeShift == 0xFF) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: announce() called but typeShift not set");
    return;
  }
  Command cmd;
  cmd.commandType = ProtocolConstants::ANNOUNCE_COMMAND;
  cmd.params[0] = ownTypeShift;
  cm->dispatch(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// Heartbeat
// ─────────────────────────────────────────────────────────────────────────────

void OpxSession::setHeartbeatInterval(uint32_t intervalMs) {
  heartbeatIntervalMs = intervalMs;
}

void OpxSession::setDeviceTimeout(uint32_t timeoutMs) {
  deviceRegistry.setDeviceTimeout(timeoutMs);
}

void OpxSession::setAnnounceInterval(uint32_t intervalMs) {
  announceIntervalMs = intervalMs;
}

// dispatch() and getAllSettings() are defined inline in OpxSession.h

// ─────────────────────────────────────────────────────────────────────────────
// Device Access
// ─────────────────────────────────────────────────────────────────────────────
// getDevice<TController>() is a template, defined inline in OpxSession.h

CommandRegistry &OpxSession::registry() { return reg; }

// ─────────────────────────────────────────────────────────────────────────────
// Telemetry Management (session as identity)
// ─────────────────────────────────────────────────────────────────────────────

bool OpxSession::registerTelemetry(uint16_t sourceID, TriggerConfig trigger) {
  ensureTelemetryManager();
  return telemetryManager->registerSource(sourceID, trigger);
}

bool OpxSession::updateTelemetry(uint16_t sourceID, const ValueSource &value) {
  if (!telemetryManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: updateTelemetry() before registerTelemetry()");
    return false;
  }
  telemetryManager->update(sourceID, value);
  return true;
}

bool OpxSession::sendTelemetryNow(uint16_t sourceID) {
  if (!telemetryManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: sendTelemetryNow() before registerTelemetry()");
    return false;
  }
  return telemetryManager->sendOne(sourceID);
}

bool OpxSession::setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger) {
  if (!telemetryManager) {
    return false;
  }
  return telemetryManager->setTrigger(sourceID, trigger);
}

bool OpxSession::enableTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->enable(sourceID);
}

bool OpxSession::disableTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->disable(sourceID);
}

bool OpxSession::unregisterTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->unregisterSource(sourceID);
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings Management (session as identity)
// ─────────────────────────────────────────────────────────────────────────────

bool OpxSession::registerSetting(uint16_t settingID, ValueType type) {
  ensureSettingsManager();
  return settingsManager->registerSetting(settingID, type);
}

bool OpxSession::updateSetting(uint16_t settingID, const ValueSource &value,
                               bool broadcast) {
  if (!settingsManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: updateSetting() before registerSetting()");
    return false;
  }
  return settingsManager->update(settingID, value, broadcast);
}

bool OpxSession::attachSettingCallback(
    uint16_t settingID, SettingsManager::SettingChangedCallback cb,
    void *context) {
  if (!settingsManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: attachSettingCallback() before registerSetting()");
    return false;
  }
  return settingsManager->attachCallback(settingID, cb, context);
}

void OpxSession::onAnySettingChanged(SettingsManager::SettingChangedCallback cb,
                                     void *context) {
  ensureSettingsManager();
  settingsManager->onAnySettingChanged(cb, context);
}

void OpxSession::broadcastAllSettings() {
  if (settingsManager)
    settingsManager->broadcastAll();
}

void OpxSession::broadcastOneSetting(uint16_t settingID) {
  if (settingsManager)
    settingsManager->broadcastOne(settingID);
}

const SettingsData *OpxSession::getSetting(uint16_t settingID) const {
  if (!settingsManager)
    return nullptr;
  return settingsManager->get(settingID);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool OpxSession::slotOccupied(OpxTransportID id) const {
  return findSlot(id) != nullptr;
}

OpxSession::TransportSlot *OpxSession::findSlot(OpxTransportID id) {
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    if (slots[i].active && slots[i].id == id) {
      return &slots[i];
    }
  }
  return nullptr;
}

const OpxSession::TransportSlot *OpxSession::findSlot(OpxTransportID id) const {
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    if (slots[i].active && slots[i].id == id) {
      return &slots[i];
    }
  }
  return nullptr;
}

void OpxSession::ensureCommunicationManager() {
  if (cm.has_value()) {
    return;
  }
  // rewireHandlers() is called immediately after construction so that any
  // handlers registered before connect*() was called are not silently lost.
  // CommunicationManager starts with no callbacks — we must re-register them.
  cm.emplace(&encoder, &tm, &sendMutex, &listenMutex);
  rewireHandlers();
}

void OpxSession::ensureTelemetryManager() {
  if (telemetryManager)
    return;
  ensureCommunicationManager();
  telemetryManager = new TelemetryManager(&clock, &cm.value());
}

void OpxSession::ensureSettingsManager() {
  if (settingsManager)
    return;
  ensureCommunicationManager();
  settingsManager = new SettingsManager(&cm.value());
  cm->onSettingReceived(settingBridge, this);
}

bool OpxSession::addTransport(ITransport *transport, OpxTransportID id) {
  // Find an inactive slot to store this transport
  TransportSlot *slot = nullptr;
  for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
    if (!slots[i].active) {
      slot = &slots[i];
      break;
    }
  }

  if (slot == nullptr) {
    LOG(LogLevel::OP_ERROR, "OpxSession: no free slot available.");
    delete transport;
    return false;
  }

  // cm must exist before the transport is added to TransportManager because
  // cm registered its frame callback on TransportManager in its constructor.
  // Without cm, received frames would have nowhere to go.
  ensureCommunicationManager();

  if (!tm.add(transport, static_cast<uint8_t>(id))) {
    LOG(LogLevel::OP_ERROR, "OpxSession: TransportManager rejected transport.");
    delete transport;
    return false;
  }

  slot->transport = transport;
  slot->id = id;
  slot->active = true;
  activeSlots++;

  // Start background threads on the first transport
  if (activeSlots == 1) {
    startThreads();
  }

  if (ownTypeShift != 0xFF) {
    announce();
  }

  return true;
}

void OpxSession::removeTransport(OpxTransportID id) {
  TransportSlot *slot = findSlot(id);
  if (slot == nullptr) {
    LOG(LogLevel::OP_WARNING,
        "OpxSession: disconnect() called for inactive slot.");
    return;
  }

  // If this is the last slot, stop threads before removing the transport.
  // This ensures the listener thread is not calling accumulate() on a
  // transport we are about to delete.
  if (activeSlots == 1) {
    stopThreads();
  }

  tm.remove(static_cast<uint8_t>(id));
  delete slot->transport;
  slot->transport = nullptr;
  slot->active = false;
  activeSlots--;
}

void OpxSession::rewireHandlers() {
  if (!cm.has_value())
    return;

  cm->onCommandReceived(commandBridge, this);
  if (telemetryHandler)
    cm->onTelemetryReceived(telemetryBridge, this);
  if (responseHandler)
    cm->onResponseReceived(responseBridge, this);
  if (settingHandler)
    cm->onSettingReceived(settingBridge, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Background Threads
// ─────────────────────────────────────────────────────────────────────────────

void OpxSession::startThreads() {
  if (running)
    return; // already started

  running = true;

  listenerThread = std::thread([this]() {
    while (running) {
      cm->listen();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  processingThread = std::thread([this]() {
    while (running) {
      cm->processCommands();
      cm->processResponses();

      if (telemetryManager)
        telemetryManager->send();

      if (isAnyConnected()) {
        // Send heartbeat if interval has elapsed
        const uint32_t now = clock.millis();
        if (now - lastHeartbeatSentMs >= heartbeatIntervalMs) {
          Command hb;
          hb.commandType = ProtocolConstants::HEARTBEAT_COMMAND;
          hb.params[0] = ownTypeShift;
          cm->dispatchCommandToAll(hb);
          lastHeartbeatSentMs = now;
        }

        if (ownTypeShift != 0xFF &&
            now - lastAnnounceSentMs >= announceIntervalMs) {
          announce();
          lastAnnounceSentMs = now;
        }
      }

      deviceRegistry.checkTimeouts();

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
}

void OpxSession::stopThreads() {
  if (!running)
    return;

  running = false;

  if (listenerThread.joinable())
    listenerThread.join();
  if (processingThread.joinable())
    processingThread.join();
}

// ─────────────────────────────────────────────────────────────────────────────
// Static Bridge Functions
// ─────────────────────────────────────────────────────────────────────────────
// These adapt CommunicationManager's C-style callbacks (function pointer +
// void* context) back into calls on the owning OpxSession instance.

void OpxSession::telemetryBridge(const Telemetry &telemetry,
                                 uint8_t sourceTransportID, void *context) {
  auto *session = static_cast<OpxSession *>(context);
  if (session->telemetryHandler) {
    session->telemetryHandler(telemetry, sourceTransportID);
  }
}

void OpxSession::commandBridge(const Command &cmd, const uint8_t &seqNum,
                               uint8_t sourceTransportID, void *context) {
  auto *session = static_cast<OpxSession *>(context);

  if (cmd.commandType == ProtocolConstants::DISCOVER_COMMAND) {
    session->announce();
    if (session->discoverHook) {
      session->discoverHook(cmd, sourceTransportID);
    }
    return;
  }

  // Handle protocol-level commands
  if (cmd.commandType == ProtocolConstants::ANNOUNCE_COMMAND) {
    const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
    session->deviceRegistry.handleAnnounce(peerTypeShift, sourceTransportID);
    if (session->announceHook) {
      session->announceHook(cmd, sourceTransportID);
    }
    return;
  }

  if (cmd.commandType == ProtocolConstants::HEARTBEAT_COMMAND) {
    const uint8_t senderTypeShift = static_cast<uint8_t>(cmd.params[0]);
    if (senderTypeShift != 0xFF) {
      session->deviceRegistry.markAlive(senderTypeShift);
    }

    Command ack;
    ack.commandType = ProtocolConstants::HEARTBEAT_ACK;
    ack.params[0] = session->ownTypeShift;
    if (session->cm.has_value()) {
      session->cm->dispatch(ack, sourceTransportID);
    }

    if (session->heartbeatHook) {
      session->heartbeatHook(cmd, sourceTransportID);
    }

    return;
  }

  if (cmd.commandType == ProtocolConstants::HEARTBEAT_ACK) {
    const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
    session->deviceRegistry.markAlive(peerTypeShift);
    if (session->heartbeatAckHook) {
      session->heartbeatAckHook(cmd, sourceTransportID);
    }
    return;
  }

  if (session->settingsManager) {
    const uint8_t category = (cmd.commandType >> 8) & 0x07;
    const bool isSettingCmd =
        (category == 0x2 || category == 0x3) ||
        cmd.commandType == ProtocolConstants::GET_ALL_SETTINGS_COMMAND;
    if (isSettingCmd) {
      session->settingsManager->handleCommand(cmd, sourceTransportID);
      return;
    }
  }

  if (session->commandHandler) {
    session->commandHandler(cmd, seqNum, sourceTransportID);
  }
}

void OpxSession::responseBridge(const CommandResponse &response,
                                uint8_t sourceTransportID, void *context) {
  auto *session = static_cast<OpxSession *>(context);
  if (session->responseHandler) {
    session->responseHandler(response, sourceTransportID);
  }
}

void OpxSession::settingBridge(const SettingsData &setting,
                               uint8_t sourceTransportID, void *context) {
  auto *session = static_cast<OpxSession *>(context);
  if (session->settingHandler) {
    session->settingHandler(setting, sourceTransportID);
  }
}

#endif
