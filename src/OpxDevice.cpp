//
// Created by dunamis on 06/05/2026.
//

//
// OpxDevice.cpp
// OmniPlexus (opx) - Embedded Device Facade Implementation
//

#include "opx/core/OpxDevice.h"

#include "opx/constants/ProtocolConstants.h"

#ifdef ARDUINO
#ifdef ESP32
void opxListenTask(void *param) {
    OpxDevice *device = static_cast<OpxDevice *>(param);

    for (;;) {
        // Check stop flag before acquiring any mutex — ensures we never
        // hold listenMutex when the task is about to be destroyed.
        if (device->listenTaskShouldStop) {
            // Signal endAll() / end() that the task has exited cleanly
            xSemaphoreGive(device->listenTaskDoneSem);
            vTaskDelete(nullptr); // self-delete
            return;
        }

        if (device->cm) {
            device->cm->listen();
        }

        vTaskDelay(1); // yield — prevents starving other tasks
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
    // Semaphore is created once at construction and reused across
    // begin/end cycles — avoids repeated heap allocation.
    listenTaskDoneSem= xSemaphoreCreateBinary();
    #endif

    #ifdef CDNC_ENABLED
    cdncActive=false;
    cdncManager=nullptr;
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

// beginSerial is a template — implementation lives in the header.

#ifdef ESP32
bool OpxDevice::beginWiFi(uint16_t port, uint32_t stackSize) {
    if (slotOccupied(OpxDeviceTransportID::OPX_WIFI)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: WIFI slot already occupied. Call end(WIFI) first.");
        return false;
    }

    auto *transport = new EspWiFiTransport(port);
    if (!addTransport(transport, OpxDeviceTransportID::OPX_WIFI)) {
        // addTransport deletes transport on failure
        return false;
    }

    // Start the shared listen task if it is not already running.
    // The task serves all active transports — only one is ever needed.
    if (listenTaskHandle == nullptr) {
        listenTaskShouldStop = false;
        xTaskCreate(
            opxListenTask,
            "OpxListen",
            stackSize,
            this,
            1,
            &listenTaskHandle
        );
    }

    return true;
}

bool OpxDevice::beginHttpServer(uint16_t port, uint32_t stackSize) {
    if (slotOccupied(OpxDeviceTransportID::OPX_HTTP)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: HTTP slot already occupied. Call end(HTTP) first.");
        return false;
    }

    auto *transport = new EspHttpTransport(port);
    if (!addTransport(transport, OpxDeviceTransportID::OPX_HTTP)) {
        return false;
    }

    // Same shared listen task — start only if not already running
    if (listenTaskHandle == nullptr) {
        listenTaskShouldStop = false;
        xTaskCreate(
            opxListenTask,
            "OpxListen",
            stackSize,
            this,
            1,
            &listenTaskHandle
        );
    }

    return true;
}

bool OpxDevice::beginHttpClient(const char *host, uint16_t port) {
    if (slotOccupied(OpxDeviceTransportID::OPX_HTTP)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: HTTP slot already occupied. Call end(HTTP) first.");
        return false;
    }

    auto *transport = new EspHttpTransport(host, port);
    if (!addTransport(transport, OpxDeviceTransportID::OPX_HTTP)) {
        return false;
    }

    // HTTP client send() is synchronous — no listen task needed for outbound-only.
    // If the caller also wants to receive frames via HTTP, they should use
    // beginHttpServer() instead.

    return true;
}

bool OpxDevice::connectWiFi(const char *host, uint16_t port,
                            uint8_t maxReconnectAttempts,
                            uint32_t reconnectDelayMs) {
    if (slotOccupied(OpxDeviceTransportID::OPX_WIFI)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: WIFI slot already occupied. Call end(WIFI) first.");
        return false;
    }

    auto *transport = new EspWiFiTransport(host, port,
                                           maxReconnectAttempts,
                                           reconnectDelayMs);

    if (!addTransport(transport, OpxDeviceTransportID::OPX_WIFI)) {
        // addTransport deletes transport on failure
        return false;
    }

    // Start the shared listen task if not already running.
    // Same task serves both beginWiFi and connectWiFi — only one is ever needed.
    if (listenTaskHandle == nullptr) {
        listenTaskShouldStop = false;
        xTaskCreate(
            opxListenTask,
            "OpxListen",
            4096,
            this,
            1,
            &listenTaskHandle
        );
    }

    return true;
                            }

                            bool OpxDevice::connectHttp(const char *host, uint16_t port) {
                                return beginHttpClient(host, port);
                            }
                            #endif // ESP32

                            #ifdef CDNC_ENABLED
                            bool OpxDevice::beginCDnC() {
                                if (cdncActive) {
                                    LOG(LogLevel::OP_WARNING, "OpxDevice: CDnC already active. Call endCDnC() first.");
                                    return false;
                                }

                                ensureCommunicationManager();

                                cdncManager = new CDnCManager();
                                if (!cdncManager->init(&tm)) {
                                    LOG(LogLevel::OP_ERROR, "OpxDevice: CDnCManager failed to initialize.");
                                    delete cdncManager;
                                    cdncManager = nullptr;
                                    return false;
                                }

                                cdncActive = true;
                                return true;
                            }
                            #endif // CDNC_ENABLED

                            // ─────────────────────────────────────────────────────────────────────────────
                            // Transport Teardown
                            // ─────────────────────────────────────────────────────────────────────────────

                            void OpxDevice::end(OpxDeviceTransportID id) {
                                removeTransport(id);

                                #ifdef ESP32
                                // If no non-CDnC transports remain active and CDnC is also inactive,
                                // stop the listen task. Otherwise keep it running for remaining transports.
                                #ifdef CDNC_ENABLED
                                if (activeSlotCount== 0 && !cdncActive) {
                                    #else
                                    if (activeSlotCount== 0) {
                                        #endif
                                        stopListenTask();
                                    }
                                    #endif
                                }

                                #ifdef CDNC_ENABLED
                                void OpxDevice::endCDnC() {
                                    if (!cdncActive) return;

                                    // Unregister all CDnC slots from TransportManager before deleting manager.
                                    // CDnCManager registered 17 slots (0–15 + broadcast) — remove them all.
                                    for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++) {
                                        tm.remove(i);
                                    }
                                    tm.remove(CDNC_TRANSPORT_ID_BROADCAST);

                                    delete cdncManager;
                                    cdncManager = nullptr;
                                    cdncActive = false;

                                    #ifdef ESP32
                                    if (activeSlotCount== 0) {
                                        stopListenTask();
                                    }
                                    #endif
                                }
                                #endif // CDNC_ENABLED

                                void OpxDevice::endAll() {


                                    #ifdef ESP32
                                    stopListenTask();
                                    #endif

                                    // Remove and delete all non-CDnC transport slots
                                    for (uint8_t i =0; i<MAX_DEVICE_TRANSPORTS; i++) {
                                        if (slots[i].active) {
                                            tm.remove(static_cast<uint8_t>(slots[i].id));
                                            delete slots[i].transport;
                                            slots[i].transport = nullptr;
                                            slots[i].active    = false;
                                        }
                                    }
                                    activeSlotCount=0;

                                    #ifdef CDNC_ENABLED
                                    // Tear down CDnC without calling endCDnC() to avoid double stopListenTask()
                                    if (cdncActive) {
                                        for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++) {
                                            tm.remove(i);
                                        }
                                        tm.remove(CDNC_TRANSPORT_ID_BROADCAST);
                                        delete cdncManager;
                                        cdncManager = nullptr;
                                        cdncActive = false;
                                    }
                                    #endif

                                    delete settingsManager;
                                    settingsManager=nullptr;

                                    // Destroy telemetryManager before cm — it holds a pointer to cm internally
                                    delete telemetryManager;
                                    telemetryManager=nullptr;

                                    // Safe to destroy now — listen task is stopped and no callbacks are in flight
                                    delete cm;
                                    cm=nullptr;
                                }

                                // ─────────────────────────────────────────────────────────────────────────────
                                // Event Handlers
                                // ─────────────────────────────────────────────────────────────────────────────

                                void OpxDevice::onCommand(CommandHandler handler, void *context) {
                                    commandHandler = handler;
                                    commandHandlerContext = context;
                                    // If cm already exists, register immediately.
                                    // If not, rewireHandlers() will register when cm is constructed.
                                }

                                void OpxDevice::onResponse(ResponseHandler handler, void *context) {
                                    responseHandler = handler;
                                    responseHandlerContext = context;
                                    if (cm) cm->onResponseReceived(responseBridge, this);
                                }

                                void OpxDevice::onIncomingTelemetry(TelemetryHandler handler, void *context) {
                                    telemetryHandler = handler;
                                    telemetryHandlerContext = context;
                                    if (cm) cm->onTelemetryReceived(telemetryBridge, this);
                                }

                                // ─────────────────────────────────────────────────────────────────────────────
                                // Main Loop
                                // ─────────────────────────────────────────────────────────────────────────────

                                void OpxDevice::update() {
                                    if (!cm) return;

                                    #ifdef ESP32
                                    // On ESP32, the listen task handles cm->listen() for WiFi and HTTP transports
                                    // in the background. However if only a serial or CDnC transport is active
                                    // (no listen task was started), we must call listen() here on the main thread.
                                    if (listenTaskHandle== nullptr) {
                                        cm->listen();
                                    }
                                    #else
                                    // On all non-ESP32 platforms, everything runs on the main thread sequentially.
                                    cm->listen();
                                    #endif

                                    cm->processCommands();
                                    cm->processResponses();

                                    // TelemetryManager::send() is always safe to call — if no sources are
                                    // registered it returns immediately with zero overhead.
                                    if (telemetryManager) {
                                        telemetryManager->send();
                                    }

                                    // Check heartbeat timeout
                                    if (heartbeatReceived &&!connectionLost) {
                                        const uint32_t now = clock.millis();
                                        const uint32_t elapsed = now - lastHeartbeatMs;
                                        if (elapsed >= heartbeatTimeoutMs) {
                                            connectionLost = true;
                                            if (connectionLostCallback) {
                                                connectionLostCallback();
                                            }
                                        }
                                    }
                                }

                                #ifdef CDNC_ENABLED
                                void OpxDevice::exchangeCDnC() {
                                    // Drive the physical parallel bus. Must be called before update() every loop.
                                    cdnc_exchange();
                                }
                                #endif

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
                                        LOG(LogLevel::OP_ERROR, "OpxDevice: sendResponse() called before begin*().");
                                        return false;
                                    }
                                    return cm->sendResponse(response);
                                }

                                bool OpxDevice::sendResponse(uint8_t seqNum, uint16_t commandType,
                                                             ProtocolConstants::ResponseStatus status) {
                                    if (!cm) {
                                        LOG(LogLevel::OP_ERROR, "OpxDevice: sendResponse() called before begin*().");
                                        return false;
                                    }
                                    return cm->sendResponse(seqNum, commandType, status);
                                                             }

                                                             bool OpxDevice::sendTelemetry(const Telemetry &telemetry) {
                                                                 if (!cm) {
                                                                     LOG(LogLevel::OP_ERROR, "OpxDevice: sendTelemetry() called before begin*().");
                                                                     return false;
                                                                 }
                                                                 return cm->dispatchTelemetry(telemetry);
                                                             }

                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             // Forwarding
                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             void OpxDevice::setTypeShift(uint8_t typeShift) {
                                                                 ownTypeShift = typeShift;
                                                             }

                                                             bool OpxDevice::forwardBetween(uint8_t transportA, uint8_t transportB) {
                                                                 for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
                                                                     if (!forwardingPairs[i].active) {
                                                                         forwardingPairs[i].transportA = transportA;
                                                                         forwardingPairs[i].transportB = transportB;
                                                                         forwardingPairs[i].active = true;
                                                                         // wire up forwarding bridge if cm exists
                                                                         if (cm) cm->onForwardFrame(forwardBridge, this);
                                                                         return true;
                                                                     }
                                                                 }
                                                                 LOG(LogLevel::OP_ERROR, "OpxDevice: max forwarding pairs reached");
                                                                 return false;
                                                             }

                                                             uint8_t OpxDevice::extractTypeShift(const RawData &frame) {
                                                                 if (frame.size < 2) return 0xFF;

                                                                 const ProtocolConstants::FrameType type =
                                                                 ProtocolConstants::decodeType(frame.data[0]);

                                                                 switch (type) {
                                                                     case ProtocolConstants::FrameType::COMMAND:
                                                                     case ProtocolConstants::FrameType::RESPONSE: {
                                                                         // layout: [header][seqNum][cmdType_lo][cmdType_hi]
                                                                         if (frame.size < 4) return 0xFF;
                                                                         const uint16_t cmdType =
                                                                         static_cast<uint16_t>(frame.data[2]) |
                                                                         (static_cast<uint16_t>(frame.data[3]) << 8);
                                                                         return (cmdType >> 11) & 0x1F;
                                                                     }
                                                                     case ProtocolConstants::FrameType::TELEMETRY:
                                                                     case ProtocolConstants::FrameType::SETTING: {
                                                                         // layout: [header][id_lo][id_hi]
                                                                         if (frame.size < 3) return 0xFF;
                                                                         const uint16_t id =
                                                                         static_cast<uint16_t>(frame.data[1]) |
                                                                         (static_cast<uint16_t>(frame.data[2]) << 8);
                                                                         return (id >> 8) & 0xFF;
                                                                     }
                                                                     default:
                                                                         return 0xFF; // protocol-level — broadcast, process and forward
                                                                 }
                                                             }

                                                             void OpxDevice::handleForwarding(const TaggedFrame &frame) {
                                                                 const uint8_t frameTypeShift = extractTypeShift(frame.frame);
                                                                 bool isProtocolLevel = (frameTypeShift == 0xFF);
                                                                 if (!isProtocolLevel &&
                                                                     ProtocolConstants::decodeType(frame.frame.data[0]) == ProtocolConstants::FrameType::COMMAND &&
                                                                     frame.frame.size >= 4) {
                                                                     const uint16_t cmdType =
                                                                     static_cast<uint16_t>(frame.frame.data[2]) |
                                                                     (static_cast<uint16_t>(frame.frame.data[3]) << 8);
                                                                 isProtocolLevel = ProtocolConstants::isProtocolLevelCommand(cmdType);
                                                                     }
                                                                     const bool isForMe = (ownTypeShift != 0xFF) && (frameTypeShift == ownTypeShift);

                                                                     // Forward if not for me, or if protocol-level (broadcast — forward AND process)
                                                                     if (!isForMe || isProtocolLevel) {
                                                                         // Convert RawData → SerializedData before forwarding
                                                                         SerializedData toForward;
                                                                         if (frame.frame.size > ProtocolConstants::MAX_FRAME_SIZE) {
                                                                             LOG(LogLevel::OP_ERROR, "OpxDevice: frame too large to forward");
                                                                             return;
                                                                         }
                                                                         memcpy(toForward.data, frame.frame.data, frame.frame.size);
                                                                         toForward.size = frame.frame.size;

                                                                         for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
                                                                             if (!forwardingPairs[i].active) continue;

                                                                             const uint8_t src = frame.transportID;
                                                                             const uint8_t a = forwardingPairs[i].transportA;
                                                                             const uint8_t b = forwardingPairs[i].transportB;

                                                                             if (src == a) {
                                                                                 tm.send(toForward, b);
                                                                             } else if (src == b) {
                                                                                 tm.send(toForward, a);
                                                                             }
                                                                         }
                                                                     }
                                                             }

                                                             void OpxDevice::forwardBridge(const TaggedFrame &frame, void *context) {
                                                                 auto *device = static_cast<OpxDevice *>(context);
                                                                 device->handleForwarding(frame);
                                                             }

                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             // DISCOVERY
                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             void OpxDevice::announce() {
                                                                 if (!cm) return;
                                                                 if (ownTypeShift == 0xFF) {
                                                                     LOG(LogLevel::OP_WARNING, "OpxDevice: announce() called but typeShift not set");
                                                                     return;
                                                                 }
                                                                 Command cmd;
                                                                 cmd.commandType = ProtocolConstants::ANNOUNCE_COMMAND;
                                                                 cmd.params[0] = ownTypeShift;
                                                                 cm->dispatch(cmd);
                                                             }

                                                             void OpxDevice::discover() {
                                                                 if (!cm) return;
                                                                 Command cmd;
                                                                 cmd.commandType = ProtocolConstants::DISCOVER_COMMAND;
                                                                 cm->dispatchCommandToAll(cmd);
                                                             }

                                                             void OpxDevice::onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb, void *context) {
                                                                 deviceRegistry.onDeviceConnected(cb, context);
                                                             }

                                                             void OpxDevice::onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb, void *context) {
                                                                 deviceRegistry.onDeviceDisconnected(cb, context);
                                                             }


                                                             bool OpxDevice::isDeviceConnected(uint8_t typeShift) const {
                                                                 return deviceRegistry.isConnected(typeShift);
                                                             }

                                                             uint8_t OpxDevice::transportIDFor(uint8_t typeShift) const {
                                                                 return deviceRegistry.transportIDFor(typeShift);
                                                             }

                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             // HeartBeat
                                                             // ─────────────────────────────────────────────────────────────────────────────


                                                             void OpxDevice::onConnectionLost(ConnectionLostHandler callback) {
                                                                 connectionLostCallback = callback;
                                                             }

                                                             void OpxDevice::setHeartbeatTimeout(uint32_t timeoutMs) {
                                                                 heartbeatTimeoutMs = timeoutMs;
                                                             }

                                                             // ─────────────────────────────────────────────────────────────────────────────
                                                             // Outgoing Telemetry Management
                                                             // ─────────────────────────────────────────────────────────────────────────────

                                                             bool OpxDevice::registerTelemetry(uint16_t sourceID, TriggerConfig trigger) {
                                                                 ensureTelemetryManager();
                                                                 return telemetryManager->registerSource(sourceID, trigger);
                                                             }

                                                             bool OpxDevice::updateTelemetry(uint16_t sourceID, const ValueSource &value) {
                                                                 if (!telemetryManager) {
                                                                     LOG(LogLevel::OP_WARNING, "OpxDevice: updateTelemetry() called before registerTelemetry().");
                                                                     return false;
                                                                 }
                                                                 telemetryManager->update(sourceID, value);
                                                                 return true;
                                                             }

                                                             bool OpxDevice::sendTelemetryNow(uint16_t sourceID) {
                                                                 if (!telemetryManager) {
                                                                     LOG(LogLevel::OP_WARNING, "OpxDevice: sendTelemetryNow() called before registerTelemetry().");
                                                                     return false;
                                                                 }
                                                                 return telemetryManager->sendOne(sourceID);
                                                             }

                                                             bool OpxDevice::setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger) {
                                                                 if (!telemetryManager) return false;
                                                                 return telemetryManager->setTrigger(sourceID, trigger);
                                                             }

                                                             bool OpxDevice::enableTelemetry(uint16_t sourceID) {
                                                                 if (!telemetryManager) return false;
                                                                 return telemetryManager->enable(sourceID);
                                                             }

                                                             bool OpxDevice::disableTelemetry(uint16_t sourceID) {
                                                                 if (!telemetryManager) return false;
                                                                 return telemetryManager->disable(sourceID);
                                                             }

                                                             bool OpxDevice::unregisterTelemetry(uint16_t sourceID) {
                                                                 if (!telemetryManager) return false;
                                                                 return telemetryManager->unregisterSource(sourceID);
                                                             }

                                                             bool OpxDevice::registerSetting(uint16_t settingID, ValueType type) {
                                                                 ensureSettingsManager();
                                                                 return settingsManager->registerSetting(settingID, type);
                                                             }

                                                             //Setting management methods:
                                                             bool OpxDevice::updateSetting(uint16_t settingID, const ValueSource &value, bool broadcast) {
                                                                 if (!settingsManager) {
                                                                     LOG(LogLevel::OP_WARNING, "OpxDevice: updateSetting() called before registerSetting().");
                                                                     return false;
                                                                 }
                                                                 return settingsManager->update(settingID, value, broadcast);
                                                             }


                                                             bool OpxDevice::attachSettingCallback(uint16_t settingID,
                                                                                                   SettingsManager::SettingChangedCallback cb,
                                                                                                   void *context) {
                                                                 if (!settingsManager) {
                                                                     LOG(LogLevel::OP_WARNING, "OpxDevice: attachSettingCallback() called before registerSetting().");
                                                                     return false;
                                                                 }
                                                                 #ifndef OPX_PLATFORM_AVR
                                                                 return settingsManager->attachCallback(settingID, cb, context);
                                                                 #else
                                                                 return false;
                                                                 #endif
                                                                                                   }

                                                                                                   void OpxDevice::onAnySettingChanged(SettingsManager::SettingChangedCallback cb, void *context) {
                                                                                                       ensureSettingsManager();
                                                                                                       settingsManager->onAnySettingChanged(cb, context);
                                                                                                   }

                                                                                                   void OpxDevice::broadcastAllSettings() {
                                                                                                       if (!settingsManager) return;
                                                                                                       settingsManager->broadcastAll();
                                                                                                   }

                                                                                                   void OpxDevice::broadcastOneSetting(uint16_t settingID) {
                                                                                                       if (!settingsManager) return;
                                                                                                       settingsManager->broadcastOne(settingID);
                                                                                                   }

                                                                                                   const SettingsData *OpxDevice::getSetting(uint16_t settingID) const {
                                                                                                       if (!settingsManager) return nullptr;
                                                                                                       return settingsManager->get(settingID);
                                                                                                   }

                                                                                                   // ─────────────────────────────────────────────────────────────────────────────
                                                                                                   // Escape Hatch
                                                                                                   // ─────────────────────────────────────────────────────────────────────────────

                                                                                                   CommunicationManager *OpxDevice::comms() {
                                                                                                       return cm;
                                                                                                   }

                                                                                                   // ─────────────────────────────────────────────────────────────────────────────
                                                                                                   // Internal Helpers
                                                                                                   // ─────────────────────────────────────────────────────────────────────────────

                                                                                                   OpxDevice::TransportSlot *OpxDevice::findSlot(OpxDeviceTransportID id) {
                                                                                                       for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
                                                                                                           if (slots[i].active && slots[i].id == id) return &slots[i];
                                                                                                       }
                                                                                                       return nullptr;
                                                                                                   }

                                                                                                   bool OpxDevice::slotOccupied(OpxDeviceTransportID id) const {
                                                                                                       for (uint8_t i = 0; i < MAX_DEVICE_TRANSPORTS; i++) {
                                                                                                           if (slots[i].active && slots[i].id == id) return true;
                                                                                                       }
                                                                                                       return false;
                                                                                                   }

                                                                                                   void OpxDevice::ensureCommunicationManager() {
                                                                                                       if (cm) return;

                                                                                                       cm = new CommunicationManager(&encoder, &tm, &sendMutex, &listenMutex);

                                                                                                       // Register any handlers that were set before begin*() was called.
                                                                                                       // CommunicationManager starts with no callbacks — we must wire them now.
                                                                                                       rewireHandlers();
                                                                                                   }

                                                                                                   void OpxDevice::ensureTelemetryManager() {
                                                                                                       if (telemetryManager) return;

                                                                                                       // CommunicationManager must exist before TelemetryManager — it holds
                                                                                                       // a pointer to cm to dispatch outgoing telemetry frames.
                                                                                                       ensureCommunicationManager();

                                                                                                       telemetryManager = new TelemetryManager(&clock, cm);
                                                                                                   }

                                                                                                   void OpxDevice::ensureSettingsManager() {
                                                                                                       if (settingsManager) return;
                                                                                                       ensureCommunicationManager();
                                                                                                       settingsManager = new SettingsManager(cm);
                                                                                                       if (cm) cm->onSettingReceived(settingBridge, this);
                                                                                                   }

                                                                                                   void OpxDevice::rewireHandlers() {
                                                                                                       if (!cm) return;

                                                                                                       cm->onCommandReceived(commandBridge, this);
                                                                                                       if (responseHandler) cm->onResponseReceived(responseBridge, this);
                                                                                                       if (telemetryHandler) cm->onTelemetryReceived(telemetryBridge, this);
                                                                                                       if (settingsManager) cm->onSettingReceived(settingBridge, this);

                                                                                                       // Wire forwarding bridge if any pairs are configured
                                                                                                       for (uint8_t i = 0; i < MAX_FORWARDING_PAIRS; i++) {
                                                                                                           if (forwardingPairs[i].active) {
                                                                                                               cm->onForwardFrame(forwardBridge, this);
                                                                                                               break; // only one registration needed
                                                                                                           }
                                                                                                       }
                                                                                                   }

                                                                                                   bool OpxDevice::addTransport(ITransport *transport, OpxDeviceTransportID id) {
                                                                                                       // Find a free slot
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

                                                                                                       // cm must exist before adding to tm so its frame callback is registered.
                                                                                                       // Adding the transport to tm before or after cm construction both work —
                                                                                                       // cm registers its callback on tm in its constructor retroactively.
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

                                                                                                       // Auto-announce if typeShift is set
                                                                                                       if (ownTypeShift != 0xFF) {
                                                                                                           announce();
                                                                                                       }

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
                                                                                                       if (listenTaskHandle == nullptr) return;

                                                                                                       // Signal the task to exit cleanly — it checks this flag at the top
                                                                                                       // of its loop before acquiring listenMutex, so we never kill it
                                                                                                       // while it holds a mutex.
                                                                                                       listenTaskShouldStop = true;

                                                                                                       // Block until the task gives the semaphore (confirms it has self-deleted).
                                                                                                       // portMAX_DELAY means we wait indefinitely — the task will always exit
                                                                                                       // within one loop iteration (at most ~2ms given the vTaskDelay(1)).
                                                                                                       xSemaphoreTake(listenTaskDoneSem, portMAX_DELAY);

                                                                                                       listenTaskHandle = nullptr;
                                                                                                       listenTaskShouldStop = false;
                                                                                                   }
                                                                                                   #endif // ESP32

                                                                                                   // ─────────────────────────────────────────────────────────────────────────────
                                                                                                   // Static Bridge Functions
                                                                                                   //
                                                                                                   // CommunicationManager callbacks are raw function pointers with void* context.
                                                                                                   // These bridges cast context back to OpxDevice* and invoke the stored
                                                                                                   // function pointer + context pair, keeping CommunicationManager free of
                                                                                                   // any knowledge about OpxDevice.
                                                                                                   // ─────────────────────────────────────────────────────────────────────────────

                                                                                                   void OpxDevice::commandBridge(const Command &cmd,
                                                                                                                                 const uint8_t &seqNum,
                                                                                                                                 uint8_t sourceTransportID,
                                                                                                                                 void *context) {
                                                                                                       auto *device = static_cast<OpxDevice *>(context);

                                                                                                       // Extract typeShift from command ID
                                                                                                       const uint8_t cmdTypeShift = (cmd.commandType >> 11) & 0x1F;
                                                                                                       const bool isProtocolLevel = ProtocolConstants::isProtocolLevelCommand(cmd.commandType);
                                                                                                       const bool isForMe = isProtocolLevel ||
                                                                                                       (device->ownTypeShift == 0xFF) || // typeShift not set — process everything
                                                                                                       (cmdTypeShift == device->ownTypeShift);

                                                                                                       if (!isForMe) {
                                                                                                           // Frame was already forwarded by forwardBridge — nothing to do here
                                                                                                           return;
                                                                                                       }

                                                                                                       if (cmd.commandType == ProtocolConstants::DISCOVER_COMMAND) {
                                                                                                           // Respond with our own announce
                                                                                                           device->announce();
                                                                                                           return;
                                                                                                       }

                                                                                                       if (cmd.commandType == ProtocolConstants::ANNOUNCE_COMMAND) {
                                                                                                           // A peer is announcing itself — register it
                                                                                                           const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
                                                                                                           device->deviceRegistry.handleAnnounce(peerTypeShift, sourceTransportID);
                                                                                                           return;
                                                                                                       }

                                                                                                       if (cmd.commandType == ProtocolConstants::HEARTBEAT_COMMAND) {
                                                                                                           // Reset timeout timer
                                                                                                           device->lastHeartbeatMs = device->clock.millis();
                                                                                                           device->heartbeatReceived = true;
                                                                                                           device->connectionLost = false;

                                                                                                           // Send ACK back to whoever sent the heartbeat
                                                                                                           Command ack;
                                                                                                           ack.commandType = ProtocolConstants::HEARTBEAT_ACK;
                                                                                                           ack.params[0] = device->ownTypeShift; // identify ourselves
                                                                                                           if (device->cm) {
                                                                                                               device->cm->dispatch(ack, sourceTransportID);
                                                                                                           }
                                                                                                           return;
                                                                                                       }

                                                                                                       if (cmd.commandType == ProtocolConstants::HEARTBEAT_ACK) {
                                                                                                           const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
                                                                                                           device->deviceRegistry.markAlive(peerTypeShift);
                                                                                                           device->connectionLost = false;
                                                                                                           return;
                                                                                                       }

                                                                                                       // Auto-route setting commands to SettingsManager
                                                                                                       if (device->settingsManager) {
                                                                                                           const uint8_t category = (cmd.commandType >> 8) & 0x07;
                                                                                                           const bool isSettingCmd = (category == 0x2 || category == 0x3)
                                                                                                           || cmd.commandType == ProtocolConstants::GET_ALL_SETTINGS_COMMAND;
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
                                                                                                                                                                uint8_t sourceTransportID,
                                                                                                                                                                void *context) {
                                                                                                                                     auto *device = static_cast<OpxDevice *>(context);

                                                                                                                                     // Check ownership — typeShift encoded in commandType
                                                                                                                                     const uint8_t responseTypeShift = (response.commandType >> 11) & 0x1F;
                                                                                                                                     const bool isProtocolLevel = ProtocolConstants::isProtocolLevelCommand(response.commandType);
                                                                                                                                     const bool isForMe = isProtocolLevel ||
                                                                                                                                     (device->ownTypeShift == 0xFF) ||
                                                                                                                                     (responseTypeShift == device->ownTypeShift);

                                                                                                                                     if (!isForMe) return;

                                                                                                                                     if (device->responseHandler) {
                                                                                                                                         device->responseHandler(response, sourceTransportID,
                                                                                                                                                                 device->responseHandlerContext);
                                                                                                                                     }
                                                                                                                                                                }

                                                                                                                                                                void OpxDevice::telemetryBridge(const Telemetry &telemetry,
                                                                                                                                                                                                uint8_t sourceTransportID,
                                                                                                                                                                                                void *context) {
                                                                                                                                                                    auto *device = static_cast<OpxDevice *>(context);

                                                                                                                                                                    // Check ownership — typeShift is upper byte of sourceID
                                                                                                                                                                    const uint8_t telemetryTypeShift = (telemetry.sourceID >> 8) & 0xFF;
                                                                                                                                                                    const bool isForMe = (device->ownTypeShift == 0xFF) ||
                                                                                                                                                                    (telemetryTypeShift == device->ownTypeShift);

                                                                                                                                                                    if (!isForMe) return;

                                                                                                                                                                    if (device->telemetryHandler) {
                                                                                                                                                                        device->telemetryHandler(telemetry, sourceTransportID,
                                                                                                                                                                                                 device->telemetryHandlerContext);
                                                                                                                                                                    }
                                                                                                                                                                                                }

                                                                                                                                                                                                void OpxDevice::settingBridge(const SettingsData &setting,
                                                                                                                                                                                                                              uint8_t sourceTransportID,
                                                                                                                                                                                                                              void *context) {
                                                                                                                                                                                                    auto *device = static_cast<OpxDevice *>(context);

                                                                                                                                                                                                    // Check ownership — typeShift is upper byte of settingsID
                                                                                                                                                                                                    const uint8_t settingTypeShift = (setting.settingsID >> 8) & 0xFF;
                                                                                                                                                                                                    const bool isForMe = (device->ownTypeShift == 0xFF) ||
                                                                                                                                                                                                    (settingTypeShift == device->ownTypeShift);

                                                                                                                                                                                                    if (!isForMe) return;

                                                                                                                                                                                                    // Future: fire user callback for incoming setting frames from peers
                                                                                                                                                                                                                              }

                                                                                                                                                                                                                              #endif // ARDUINO
