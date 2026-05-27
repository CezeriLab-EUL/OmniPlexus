//
// Created by dunamis on 01/05/2026.
//

#include "opx/core/OpxSession.h"

#ifndef EMBEDDED_BUILD

#include "opx/transport/wifi/PcWiFiTransport.h"
#include "opx/transport/serial/PcSerialTransport.h"
#include "opx/transport/http/PcHttpTransport.h"
#include "opx/interfaces/IConnectable.h"
#include  <chrono>

OpxSession::OpxSession() {
    for (uint8_t i = 0; i < OPX_MAX_TRANSPORTS; i++) {
        slots[i].active = false;
        slots[i].transport = nullptr;
    }
}

OpxSession::~OpxSession() {
    disconnectAll();
}

bool OpxSession::connectWiFi(const char *host, uint16_t port, uint8_t maxReconnectAttempts, uint32_t reconnectDelayMs) {
    if (slotOccupied(OpxTransportID::WIFI)) {
        LOG(LogLevel::OP_WARNING, "OpxSession: WIFI slot already occupied. Call disconnect(WIFI) first.");
        return false;
    }

    auto *transport = new PcWiFiTransport(host, port, maxReconnectAttempts, reconnectDelayMs);
    // PcWiFiTransport attempts connection in its constructor but doesn't throw on
    // failure — it sets an internal flag instead. We check IConnectable here to
    // catch that failure and report it cleanly before the transport enters the slot.
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
        LOG(LogLevel::OP_WARNING, "OpxSession: SERIAL slot already occupied. Call disconnect(SERIAL) first.");
        return false;
    }

    PcSerialTransport *transport = nullptr;

    // Unlike WiFi and HTTP, PcSerialTransport throws a boost::system::system_error
    // if the port doesn't exist or is already in use. We catch it here to maintain
    // our no-exceptions contract with the frontend.

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
        LOG(LogLevel::OP_WARNING, "OpxSession: HTTP slot already occupied. Call disconnect(HTTP) first.");
        return false;
    }

    auto *transport = new PcHttpTransport(host, port);
    return addTransport(transport, OpxTransportID::HTTP);
}

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
    // cm is destroyed after transports so any final frames processed during
    // stopThreads() can still be dispatched. Controllers are cleared after cm
    // because their destructors may reference cm internals.
    cm.reset();
    controllerMap.clear();
    deviceRegistry.clear();
}

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
            const auto *connectable = dynamic_cast<const IConnectable *>(slots[i].transport);
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

void OpxSession::onTelemetry(TelemetryHandler handler) {
    telemetryHandler = std::move(handler);
    if (cm.has_value()) {
        cm->onTelemetryReceived(telemetryBridge, this);
    }
}

void OpxSession::onSetting(SettingHandler handler) {
    settingHandler = std::move(handler);
    if (cm.has_value()) {
        cm->onSettingReceived(settingBridge, this);
    }
}

void OpxSession::discover() {
    if (!cm.has_value()) return;
    Command cmd;
    cmd.commandType = ProtocolConstants::DISCOVER_COMMAND;
    cm->dispatchCommandToAll(cmd);
}

void OpxSession::onDeviceConnected(DeviceRegistry::DeviceConnectedCallback cb, void *context) {
    deviceRegistry.onDeviceConnected(cb, context);
}

void OpxSession::onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb, void *context) {
    deviceRegistry.onDeviceDisconnected(cb, context);
}

bool OpxSession::isDeviceConnected(uint8_t typeShift) const {
    return deviceRegistry.isConnected(typeShift);
}

uint8_t OpxSession::transportIDFor(uint8_t typeShift) const {
    return deviceRegistry.transportIDFor(typeShift);
}

void OpxSession::onCommand(CommandHandler handler) {
    commandHandler = std::move(handler);
}

void OpxSession::onCommandResponse(ResponseHandler handler) {
    responseHandler = std::move(handler);
    if (cm.has_value()) {
        cm->onResponseReceived(responseBridge, this);
    }
}

CommandRegistry &OpxSession::registry() {
    return reg;
}

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

    return true;
}

void OpxSession::removeTransport(OpxTransportID id) {
    TransportSlot *slot = findSlot(id);
    if (slot == nullptr) {
        LOG(LogLevel::OP_WARNING, "OpxSession: disconnect() called for inactive slot.");
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
    if (!cm.has_value()) return;

    cm->onCommandReceived(commandBridge, this);
    if (telemetryHandler) cm->onTelemetryReceived(telemetryBridge, this);
    if (responseHandler) cm->onResponseReceived(responseBridge, this);
    if (settingHandler) cm->onSettingReceived(settingBridge, this);
}

void OpxSession::startThreads() {
    if (running) return; // already started

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
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
}

void OpxSession::stopThreads() {
    if (!running) return;

    running = false;

    if (listenerThread.joinable()) listenerThread.join();
    if (processingThread.joinable()) processingThread.join();
}

void OpxSession::telemetryBridge(const Telemetry &telemetry,
                                 uint8_t sourceTransportID,
                                 void *context) {
    auto *session = static_cast<OpxSession *>(context);
    if (session->telemetryHandler) {
        session->telemetryHandler(telemetry, sourceTransportID);
    }
}

void OpxSession::settingBridge(const SettingsData &setting,
                                uint8_t sourceTransportID,
                                void *context) {
    auto *session = static_cast<OpxSession *>(context);
    if (session->settingHandler) {
        session->settingHandler(setting, sourceTransportID);
    }
}


void OpxSession::commandBridge(const Command &cmd,
                               const uint8_t &seqNum,
                               uint8_t sourceTransportID,
                               void *context) {
    auto *session = static_cast<OpxSession *>(context);
    // Handle protocol-level commands
    if (cmd.commandType == ProtocolConstants::ANNOUNCE_COMMAND) {
        const uint8_t peerTypeShift = static_cast<uint8_t>(cmd.params[0]);
        session->deviceRegistry.handleAnnounce(peerTypeShift, sourceTransportID);
        return;
    }

    if (cmd.commandType == ProtocolConstants::DISCOVER_COMMAND) {
        // PC doesn't respond to discover for now — device role only
        return;
    }

    if (session->commandHandler) {
        session->commandHandler(cmd, seqNum, sourceTransportID);
    }
}

void OpxSession::responseBridge(const CommandResponse &response,
                                uint8_t sourceTransportID,
                                void *context) {
    auto *session = static_cast<OpxSession *>(context);
    if (session->responseHandler) {
        session->responseHandler(response, sourceTransportID);
    }
}

#endif
