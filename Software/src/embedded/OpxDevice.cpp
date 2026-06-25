//
// OpxDevice.cpp
// OmniPlexus (opx) - Embedded Device Facade Implementation
//

#include "opx/embedded/core/OpxDevice.h"
#include "opx/shared/constants/ProtocolConstants.h"

#ifdef ARDUINO

#ifdef CDNC_SLAVE
CDnCSlaveTransport *CDnCSlaveTransport::_instance = nullptr;
#endif

#ifdef ESP32
void opxListenTask(void *param) {
  OpxDevice *device = static_cast<OpxDevice *>(param);
  for (;;) {
    if (device->listenTaskShouldStop) {
      xSemaphoreGive(device->listenTaskDoneSem);
      vTaskDelete(nullptr);
      return;
    }
    if (device->cm)
      device->cm->listen();
    vTaskDelay(1);
  }
}
#endif // ESP32

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

OpxDevice::OpxDevice() {
  for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
    slots[i].active = false;
    slots[i].transport = nullptr;
  }
  for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
    forwardingPairs[i].active = false;
  }
#ifdef ESP32
  listenTaskDoneSem = xSemaphoreCreateBinary();
#endif
#ifdef CDNC_MASTER
  cdncActive = false;
  cdncManager = nullptr;
  _cdncPrevAliveMask = 0;
#endif
#ifdef CDNC_SLAVE
  _cdncSlaveActive = false;
  _cdncSlaveManager = nullptr;
#endif
}

OpxDevice::~OpxDevice() {
  endAll();
#ifdef ESP32
  if (listenTaskDoneSem) {
    vSemaphoreDelete(listenTaskDoneSem);
    listenTaskDoneSem = nullptr;
  }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport Setup
// ─────────────────────────────────────────────────────────────────────────────

#ifdef ESP32
bool OpxDevice::beginWiFi(uint16_t port, uint32_t stackSize) {
  if (slotOccupied(OpxDeviceTransportID::OPX_WIFI)) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: WIFI slot already occupied.");
    return false;
  }
  auto *transport = new EspWiFiTransport(port);
  if (!addTransport(transport, OpxDeviceTransportID::OPX_WIFI))
    return false;
  if (listenTaskHandle == nullptr) {
    listenTaskShouldStop = false;
    // xTaskCreate(opxListenTask, "OpxListen", stackSize, this, 1,
    // &listenTaskHandle);
    xTaskCreatePinnedToCore(opxListenTask, "OpxListen", stackSize, this, 1,
                            &listenTaskHandle,
                            0 // pin to core 0, loop() runs on core 1
    );
  }
  return true;
}

bool OpxDevice::addRawTransport(ITransport *transport, uint8_t id) {
  ensureCommunicationManager();
  return tm.add(transport, id);
}

bool OpxDevice::beginHttpServer(uint16_t port, uint32_t stackSize) {
  if (slotOccupied(OpxDeviceTransportID::OPX_HTTP)) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: HTTP slot already occupied.");
    return false;
  }
  auto *transport = new EspHttpTransport(port);
  if (!addTransport(transport, OpxDeviceTransportID::OPX_HTTP))
    return false;
  if (listenTaskHandle == nullptr) {
    listenTaskShouldStop = false;
    // xTaskCreate(opxListenTask, "OpxListen", stackSize, this, 1,
    // &listenTaskHandle);
    xTaskCreatePinnedToCore(opxListenTask, "OpxListen", stackSize, this, 1,
                            &listenTaskHandle,
                            0 // pin to core 0, loop() runs on core 1
    );
  }
  return true;
}

bool OpxDevice::beginHttpClient(const char *host, uint16_t port) {
  if (slotOccupied(OpxDeviceTransportID::OPX_HTTP)) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: HTTP slot already occupied.");
    return false;
  }
  auto *transport = new EspHttpTransport(host, port);
  return addTransport(transport, OpxDeviceTransportID::OPX_HTTP);
}

bool OpxDevice::connectWiFi(const char *host, uint16_t port,
                            uint8_t maxReconnectAttempts,
                            uint32_t reconnectDelayMs) {
  if (slotOccupied(OpxDeviceTransportID::OPX_WIFI)) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: WIFI slot already occupied.");
    return false;
  }
  auto *transport =
      new EspWiFiTransport(host, port, maxReconnectAttempts, reconnectDelayMs);
  if (!addTransport(transport, OpxDeviceTransportID::OPX_WIFI))
    return false;
  if (listenTaskHandle == nullptr) {
    listenTaskShouldStop = false;
    // xTaskCreate(opxListenTask, "OpxListen", 4096, this, 1,
    // &listenTaskHandle);
    xTaskCreatePinnedToCore(opxListenTask, "OpxListen", 4096, this, 1,
                            &listenTaskHandle,
                            0 // pin to core 0, loop() runs on core 1
    );
  }
  return true;
}

bool OpxDevice::connectHttp(const char *host, uint16_t port) {
  return beginHttpClient(host, port);
}
#endif // ESP32

// CDnC master/slave methods are defined inline in OpxDevice.h
// so the sketch's #define CDNC_MASTER / CDNC_SLAVE is visible at compile time.

// ─────────────────────────────────────────────────────────────────────────────
// Transport Teardown
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::end(OpxDeviceTransportID id) {
  removeTransport(id);
#ifdef ESP32
#ifdef CDNC_MASTER
  if (activeSlotCount == 0 && !cdncActive)
    stopListenTask();
#else
  if (activeSlotCount == 0)
    stopListenTask();
#endif
#endif
}

// endCDnC() and exchangeCDnC() are defined inline in OpxDevice.h

void OpxDevice::endAll() {

#ifdef ESP32
  stopListenTask();
#endif
  for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
    if (slots[i].active) {
      tm.remove(static_cast<uint8_t>(slots[i].id));
      delete slots[i].transport;
      slots[i].transport = nullptr;
      slots[i].active = false;
    }
  }
  activeSlotCount = 0;

#ifdef CDNC_MASTER
  if (cdncActive) {
    for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++)
      tm.remove(i);
    tm.remove(CDNC_TRANSPORT_ID_BROADCAST);
    delete cdncManager;
    cdncManager = nullptr;
    cdncActive = false;
  }
#endif

#ifdef CDNC_SLAVE
  if (_cdncSlaveActive) {
    tm.remove(CDNC_SLAVE_TRANSPORT_ID);
    delete _cdncSlaveManager;
    _cdncSlaveManager = nullptr;
    _cdncSlaveActive = false;
  }
#endif

  delete settingsManager;
  settingsManager = nullptr;
  delete telemetryManager;
  telemetryManager = nullptr;
  delete cm;
  cm = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event Handlers
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::onCommand(CommandHandler handler, void *context) {
  commandHandler = handler;
  commandHandlerContext = context;
}

void OpxDevice::onResponse(ResponseHandler handler, void *context) {
  responseHandler = handler;
  responseHandlerContext = context;
  if (cm)
    cm->onResponseReceived(responseBridge, this);
}

void OpxDevice::onIncomingTelemetry(TelemetryHandler handler, void *context) {
  telemetryHandler = handler;
  telemetryHandlerContext = context;
  if (cm)
    cm->onTelemetryReceived(telemetryBridge, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Loop
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::update() {
  if (!cm)
    return;

#ifdef ESP32
  if (listenTaskHandle == nullptr)
    cm->listen();
#else
  cm->listen();
#endif

  cm->processCommands();
  cm->processResponses();

  if (telemetryManager)
    telemetryManager->send();

  if (heartbeatReceived && !connectionLost) {
    const uint32_t now = clock.millis();
    const uint32_t elapsed = now - lastHeartbeatMs;
    if (elapsed >= heartbeatTimeoutMs) {
      connectionLost = true;
      if (connectionLostCallback)
        connectionLostCallback();
    }
  }

#ifdef CDNC_SLAVE
  if (_cdncSlaveActive && _cdncSlaveManager)
    _cdncSlaveManager->tick();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Sending
// ─────────────────────────────────────────────────────────────────────────────

bool OpxDevice::sendCommand(const Command &cmd, uint8_t transportID) {
  if (!cm) {
    LOG(LogLevel::OP_ERROR, "OpxDevice: sendCommand() called before begin*().");
    return false;
  }
  return cm->dispatch(cmd, transportID);
}

bool OpxDevice::sendResponse(const CommandResponse &response) {
  if (!cm) {
    LOG(LogLevel::OP_ERROR,
        "OpxDevice: sendResponse() called before begin*().");
    return false;
  }
  return cm->sendResponse(response);
}

bool OpxDevice::sendResponse(uint8_t seqNum, uint16_t commandType,
                             ProtocolConstants::ResponseStatus status) {
  if (!cm) {
    LOG(LogLevel::OP_ERROR,
        "OpxDevice: sendResponse() called before begin*().");
    return false;
  }
  return cm->sendResponse(seqNum, commandType, status);
}

bool OpxDevice::sendTelemetry(const Telemetry &telemetry) {
  if (!cm) {
    LOG(LogLevel::OP_ERROR,
        "OpxDevice: sendTelemetry() called before begin*().");
    return false;
  }
  return cm->dispatchTelemetry(telemetry);
}

// ─────────────────────────────────────────────────────────────────────────────
// Forwarding
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::setTypeShift(uint8_t typeShift) { ownTypeShift = typeShift; }

bool OpxDevice::forwardBetween(uint8_t transportA, uint8_t transportB) {
  for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
    if (!forwardingPairs[i].active) {
      forwardingPairs[i].transportA = transportA;
      forwardingPairs[i].transportB = transportB;
      forwardingPairs[i].active = true;
      if (cm)
        cm->onForwardFrame(forwardBridge, this);
      return true;
    }
  }
  LOG(LogLevel::OP_ERROR, "OpxDevice: max forwarding pairs reached");
  return false;
}

uint8_t OpxDevice::extractTypeShift(const RawData &frame) {
  if (frame.size < 2)
    return 0xFF;
  const ProtocolConstants::FrameType type =
      ProtocolConstants::decodeType(frame.data[0]);
  switch (type) {
  case ProtocolConstants::FrameType::COMMAND:
  case ProtocolConstants::FrameType::RESPONSE: {
    if (frame.size < 4)
      return 0xFF;
    const uint16_t cmdType = static_cast<uint16_t>(frame.data[2]) |
                             (static_cast<uint16_t>(frame.data[3]) << 8);
    return (cmdType >> 11) & 0x1F;
  }
  case ProtocolConstants::FrameType::TELEMETRY:
  case ProtocolConstants::FrameType::SETTING: {
    if (frame.size < 3)
      return 0xFF;
    const uint16_t id = static_cast<uint16_t>(frame.data[1]) |
                        (static_cast<uint16_t>(frame.data[2]) << 8);
    return (id >> 8) & 0xFF;
  }
  default:
    return 0xFF;
  }
}

void OpxDevice::handleForwarding(const TaggedFrame &frame) {
  const uint8_t frameTypeShift = extractTypeShift(frame.frame);
  bool isProtocolLevel = (frameTypeShift == 0xFF);

  if (!isProtocolLevel &&
      ProtocolConstants::decodeType(frame.frame.data[0]) ==
          ProtocolConstants::FrameType::COMMAND &&
      frame.frame.size >= 4) {
    const uint16_t cmdType = static_cast<uint16_t>(frame.frame.data[2]) |
                             (static_cast<uint16_t>(frame.frame.data[3]) << 8);
    isProtocolLevel = ProtocolConstants::isProtocolLevelCommand(cmdType);
  }

  const bool isForMe =
      (ownTypeShift != 0xFF) && (frameTypeShift == ownTypeShift);

  if (!isForMe || isProtocolLevel) {
    SerializedData toForward;
    if (frame.frame.size > ProtocolConstants::MAX_FRAME_SIZE) {
      LOG(LogLevel::OP_ERROR, "OpxDevice: frame too large to forward");
      return;
    }
    memcpy(toForward.data, frame.frame.data, frame.frame.size);
    toForward.size = frame.frame.size;

    for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
      if (!forwardingPairs[i].active)
        continue;
      const uint8_t src = frame.transportID;
      const uint8_t a = forwardingPairs[i].transportA;
      const uint8_t b = forwardingPairs[i].transportB;
      if (src == a)
        tm.send(toForward, b);
      else if (src == b)
        tm.send(toForward, a);
    }
  }
}

void OpxDevice::forwardBridge(const TaggedFrame &frame, void *context) {
  static_cast<OpxDevice *>(context)->handleForwarding(frame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Discovery
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::announce() {
  if (!cm)
    return;
  if (ownTypeShift == 0xFF) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: announce() called but typeShift not set");
    return;
  }
  Command cmd;
  cmd.commandType = ProtocolConstants::ANNOUNCE_COMMAND;
  cmd.params[0] = ownTypeShift;
  cm->dispatch(cmd);
}

void OpxDevice::discover() {
  if (!cm)
    return;
  Command cmd;
  cmd.commandType = ProtocolConstants::DISCOVER_COMMAND;
  cm->dispatchCommandToAll(cmd);
}

void OpxDevice::onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb,
                                  void *context) {
  deviceRegistry.onDeviceConnected(cb, context);
}
void OpxDevice::onDeviceDisconnected(
    DeviceRegistry::DeviceDisconnectedCallback cb, void *context) {
  deviceRegistry.onDeviceDisconnected(cb, context);
}
bool OpxDevice::isDeviceConnected(uint8_t typeShift) const {
  return deviceRegistry.isConnected(typeShift);
}
uint8_t OpxDevice::transportIDFor(uint8_t typeShift) const {
  return deviceRegistry.transportIDFor(typeShift);
}

// ─────────────────────────────────────────────────────────────────────────────
// Heartbeat
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::onConnectionLost(ConnectionLostHandler callback) {
  connectionLostCallback = callback;
}
void OpxDevice::setHeartbeatTimeout(uint32_t timeoutMs) {
  heartbeatTimeoutMs = timeoutMs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Telemetry Management
// ─────────────────────────────────────────────────────────────────────────────

bool OpxDevice::registerTelemetry(uint16_t sourceID, TriggerConfig trigger) {
  ensureTelemetryManager();
  return telemetryManager->registerSource(sourceID, trigger);
}
bool OpxDevice::updateTelemetry(uint16_t sourceID, const ValueSource &value) {
  if (!telemetryManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: updateTelemetry() before registerTelemetry()");
    return false;
  }
  telemetryManager->update(sourceID, value);
  return true;
}
bool OpxDevice::sendTelemetryNow(uint16_t sourceID) {
  if (!telemetryManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: sendTelemetryNow() before registerTelemetry()");
    return false;
  }
  return telemetryManager->sendOne(sourceID);
}
bool OpxDevice::setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger) {
  if (!telemetryManager)
    return false;
  return telemetryManager->setTrigger(sourceID, trigger);
}
bool OpxDevice::enableTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->enable(sourceID);
}
bool OpxDevice::disableTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->disable(sourceID);
}
bool OpxDevice::unregisterTelemetry(uint16_t sourceID) {
  if (!telemetryManager)
    return false;
  return telemetryManager->unregisterSource(sourceID);
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings Management
// ─────────────────────────────────────────────────────────────────────────────

bool OpxDevice::registerSetting(uint16_t settingID, ValueType type) {
  ensureSettingsManager();
  return settingsManager->registerSetting(settingID, type);
}
bool OpxDevice::updateSetting(uint16_t settingID, const ValueSource &value,
                              bool broadcast) {
  if (!settingsManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: updateSetting() before registerSetting()");
    return false;
  }
  return settingsManager->update(settingID, value, broadcast);
}
bool OpxDevice::attachSettingCallback(
    uint16_t settingID, SettingsManager::SettingChangedCallback cb,
    void *context) {
  if (!settingsManager) {
    LOG(LogLevel::OP_WARNING,
        "OpxDevice: attachSettingCallback() before registerSetting()");
    return false;
  }
#ifndef OPX_PLATFORM_AVR
  return settingsManager->attachCallback(settingID, cb, context);
#else
  return false;
#endif
}
void OpxDevice::onAnySettingChanged(SettingsManager::SettingChangedCallback cb,
                                    void *context) {
  ensureSettingsManager();
  settingsManager->onAnySettingChanged(cb, context);
}
void OpxDevice::broadcastAllSettings() {
  if (settingsManager)
    settingsManager->broadcastAll();
}
void OpxDevice::broadcastOneSetting(uint16_t settingID) {
  if (settingsManager)
    settingsManager->broadcastOne(settingID);
}
const SettingsData *OpxDevice::getSetting(uint16_t settingID) const {
  if (!settingsManager)
    return nullptr;
  return settingsManager->get(settingID);
}

// ─────────────────────────────────────────────────────────────────────────────
// Escape Hatch
// ─────────────────────────────────────────────────────────────────────────────

CommunicationManager *OpxDevice::comms() { return cm; }

// ─────────────────────────────────────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────────────────────────────────────

OpxDevice::TransportSlot *OpxDevice::findSlot(OpxDeviceTransportID id) {
  for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
    if (slots[i].active && slots[i].id == id)
      return &slots[i];
  }
  return nullptr;
}

bool OpxDevice::slotOccupied(OpxDeviceTransportID id) const {
  for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
    if (slots[i].active && slots[i].id == id)
      return true;
  }
  return false;
}

void OpxDevice::ensureCommunicationManager() {
  if (cm)
    return;
  cm = new CommunicationManager(&encoder, &tm, &sendMutex, &listenMutex);
  rewireHandlers();
}

void OpxDevice::ensureTelemetryManager() {
  if (telemetryManager)
    return;
  ensureCommunicationManager();
  telemetryManager = new TelemetryManager(&clock, cm);
}

void OpxDevice::ensureSettingsManager() {
  if (settingsManager)
    return;
  ensureCommunicationManager();
  settingsManager = new SettingsManager(cm);
  if (cm)
    cm->onSettingReceived(settingBridge, this);
}

void OpxDevice::rewireHandlers() {
  if (!cm)
    return;
  cm->onCommandReceived(commandBridge, this);
  if (responseHandler)
    cm->onResponseReceived(responseBridge, this);
  if (telemetryHandler)
    cm->onTelemetryReceived(telemetryBridge, this);
  if (settingsManager)
    cm->onSettingReceived(settingBridge, this);
  for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
    if (forwardingPairs[i].active) {
      cm->onForwardFrame(forwardBridge, this);
      break;
    }
  }
}

bool OpxDevice::addTransport(ITransport *transport, OpxDeviceTransportID id) {
  TransportSlot *slot = nullptr;
  for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
    if (!slots[i].active) {
      slot = &slots[i];
      break;
    }
  }
  if (slot == nullptr) {
    LOG(LogLevel::OP_ERROR, "OpxDevice: no free transport slot available.");
    delete transport;
    return false;
  }
  ensureCommunicationManager();
  if (!tm.add(transport, static_cast<uint8_t>(id))) {
    LOG(LogLevel::OP_ERROR, "OpxDevice: TransportManager rejected transport.");
    delete transport;
    return false;
  }
  slot->transport = transport;
  slot->id = id;
  slot->active = true;
  activeSlotCount++;
  if (ownTypeShift != 0xFF)
    announce();
  return true;
}

void OpxDevice::removeTransport(OpxDeviceTransportID id) {
  TransportSlot *slot = findSlot(id);
  if (slot == nullptr) {
    LOG(LogLevel::OP_WARNING, "OpxDevice: end() called for inactive slot.");
    return;
  }
  tm.remove(static_cast<uint8_t>(id));
  delete slot->transport;
  slot->transport = nullptr;
  slot->active = false;
  activeSlotCount--;
}

#ifdef ESP32
void OpxDevice::stopListenTask() {
  if (listenTaskHandle == nullptr)
    return;
  listenTaskShouldStop = true;
  xSemaphoreTake(listenTaskDoneSem, portMAX_DELAY);
  listenTaskHandle = nullptr;
  listenTaskShouldStop = false;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Static Bridge Functions
// ─────────────────────────────────────────────────────────────────────────────

void OpxDevice::commandBridge(const Command &cmd, const uint8_t &seqNum,
                              uint8_t sourceTransportID, void *context) {
  auto *device = static_cast<OpxDevice *>(context);

  const uint8_t cmdTypeShift = (cmd.commandType >> 11) & 0x1F;
  const bool isProtocolLevel =
      ProtocolConstants::isProtocolLevelCommand(cmd.commandType);
  const bool isForMe = isProtocolLevel || (device->ownTypeShift == 0xFF) ||
                       (cmdTypeShift == device->ownTypeShift);

  if (!isForMe)
    return;

  if (cmd.commandType == ProtocolConstants::DISCOVER_COMMAND) {
    device->announce();
    if (device->_discoverHook)
      device->_discoverHook(cmd, sourceTransportID, device->_discoverHookCtx);
    return;
  }

  if (cmd.commandType == ProtocolConstants::ANNOUNCE_COMMAND) {
    const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
    device->deviceRegistry.handleAnnounce(peerTypeShift, sourceTransportID);
    if (device->_announceHook)
      device->_announceHook(cmd, sourceTransportID, device->_announceHookCtx);
    return;
  }

  if (cmd.commandType == ProtocolConstants::HEARTBEAT_COMMAND) {
    device->lastHeartbeatMs = device->clock.millis();
    device->heartbeatReceived = true;
    device->connectionLost = false;
    Command ack;
    ack.commandType = ProtocolConstants::HEARTBEAT_ACK;
    ack.params[0] = device->ownTypeShift;
    if (device->cm)
      device->cm->dispatch(ack, sourceTransportID);
    if (device->_heartbeatHook)
      device->_heartbeatHook(cmd, sourceTransportID, device->_heartbeatHookCtx);
    return;
  }

  if (cmd.commandType == ProtocolConstants::HEARTBEAT_ACK) {
    const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
    device->deviceRegistry.markAlive(peerTypeShift);
    device->connectionLost = false;
    if (device->_heartbeatAckHook)
      device->_heartbeatAckHook(cmd, sourceTransportID,
                                device->_heartbeatAckHookCtx);
    return;
  }

  if (device->settingsManager) {
    const uint8_t category = (cmd.commandType >> 8) & 0x07;
    const bool isSettingCmd =
        (category == 0x2 || category == 0x3) ||
        cmd.commandType == ProtocolConstants::GET_ALL_SETTINGS_COMMAND;
    if (isSettingCmd) {
      device->settingsManager->handleCommand(cmd, sourceTransportID);
      return;
    }
  }

  if (device->commandHandler) {
    device->commandHandler(cmd, seqNum, sourceTransportID,
                           device->commandHandlerContext);
  }
}

void OpxDevice::responseBridge(const CommandResponse &response,
                               uint8_t sourceTransportID, void *context) {
  auto *device = static_cast<OpxDevice *>(context);
  const uint8_t responseTypeShift = (response.commandType >> 11) & 0x1F;
  const bool isProtocolLevel =
      ProtocolConstants::isProtocolLevelCommand(response.commandType);
  const bool isForMe = isProtocolLevel || (device->ownTypeShift == 0xFF) ||
                       (responseTypeShift == device->ownTypeShift);
  if (!isForMe)
    return;
  if (device->responseHandler)
    device->responseHandler(response, sourceTransportID,
                            device->responseHandlerContext);
}

void OpxDevice::telemetryBridge(const Telemetry &telemetry,
                                uint8_t sourceTransportID, void *context) {
  auto *device = static_cast<OpxDevice *>(context);
  const uint8_t telemetryTypeShift = (telemetry.sourceID >> 8) & 0xFF;
  const bool isForMe = (device->ownTypeShift == 0xFF) ||
                       (telemetryTypeShift == device->ownTypeShift);
  if (!isForMe)
    return;
  if (device->telemetryHandler)
    device->telemetryHandler(telemetry, sourceTransportID,
                             device->telemetryHandlerContext);
}

void OpxDevice::settingBridge(const SettingsData &setting,
                              uint8_t sourceTransportID, void *context) {
  auto *device = static_cast<OpxDevice *>(context);
  const uint8_t settingTypeShift = (setting.settingsID >> 8) & 0xFF;
  const bool isForMe = (device->ownTypeShift == 0xFF) ||
                       (settingTypeShift == device->ownTypeShift);
  if (!isForMe)
    return;
  // Future: fire user callback for incoming setting frames from peers
}

#endif // ARDUINO
