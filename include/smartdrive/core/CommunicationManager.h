//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_COMMUNICATIONMANAGER_H
#define SMARTDRIVE_COMMUNICATIONMANAGER_H

#include "platform.h"
#include "../interfaces/IEncoder.h"
#include "../generated/CommandPacker.h"
#include "../interfaces/ITransport.h"
#include "../types/ProtocolTypes.h"
#include "../utils/CommandQueue.h"
#include "../utils/ResponseQueue.h"
#include "../utils/PendingAckQueue.h"
#include "../utils/Logger.h"
#include "smartdrive/interfaces/IMutex.h"

class CommunicationManager {
public:
    using CommandCallback = void (*)(const Command &cmd, const uint8_t &seqNum, void *context);
    using CommandResponseCallback = void (*)(const CommandResponse &response, void *context);
    using TelemetryCallback = void (*)(const Telemetry &telemetry, void *context);

private:
    IEncoder *encoder;
    ITransport *transport;
    CommandQueue queue;
    ResponseQueue responseQueue;
    PendingAckQueue pendingAcks;
    uint8_t nextSeqNum = ProtocolConstants::SEQ_NUM_MIN;
    CommandCallback callback = nullptr;
    CommandResponseCallback responseCallback = nullptr;
    TelemetryCallback telemetryCallback = nullptr;
    void *callbackContext = nullptr;
    void *responseCallbackContext = nullptr;
    void *telemetryCallbackContext = nullptr;
    IMutex *sendMutex = nullptr;
    IMutex *listenMutex = nullptr;

    struct MutexGuard {
        IMutex* mutex;
        explicit MutexGuard(IMutex* m) : mutex(m) {
            if (mutex) mutex->lock();
        }
        ~MutexGuard() {
            if (mutex) mutex->unlock();
        }
    };

public:
    CommunicationManager(IEncoder *encoder, ITransport *transport, IMutex *sendMutex = nullptr,
                         IMutex *listenMutex = nullptr) : encoder(encoder), transport(transport), sendMutex(sendMutex),
                                                          listenMutex(listenMutex) {
    }

    void onCommandReceived(CommandCallback cb, void *context = nullptr) {
        callback = cb;
        callbackContext = context;
    }

    void onResponseReceived(CommandResponseCallback cb, void *context = nullptr) {
        responseCallback = cb;
        responseCallbackContext = context;
    }

    void onTelemetryReceived(TelemetryCallback cb, void *context = nullptr) {
        telemetryCallback = cb;
        telemetryCallbackContext = context;
    }

    bool dispatch(const Command &cmd, bool requiresAck = false) {
        MutexGuard guard(sendMutex);

        if (!encoder || !transport) {
            LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
            return false;
        }

        uint8_t seqNum = ProtocolConstants::SEQ_NUM_FIRE_AND_FORGET;

        if (requiresAck) {
            if (pendingAcks.isFull()) {
                LOG(LogLevel::OP_WARNING, "Pending ACK queue full, can't dispatch acknowledgeable command");
                return false;
            }
            seqNum = nextSeqNum;
            nextSeqNum = (nextSeqNum >= ProtocolConstants::SEQ_NUM_MAX)
                             ? ProtocolConstants::SEQ_NUM_MIN
                             : nextSeqNum + 1;

            pendingAcks.push({seqNum, cmd.commandType});
        }

        const SerializedData frame = encoder->serializeCommand(cmd, seqNum);

        if (frame.size == 0) {
            LOG(LogLevel::OP_ERROR, "Failed to serialize command");
            return false;
        }

        return transport->send(frame);
    }

    bool dispatchTelemetry(const Telemetry &value) {
        MutexGuard guard(sendMutex);

        if (!encoder || !transport) {
            LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
            return false;
        }

        const SerializedData frame = encoder->serializeTelemetry(value);
        if (frame.size == 0) {
            LOG(LogLevel::OP_ERROR, "Failed to serialize telemetry");
            return false;
        }

        return transport->send(frame);
    }

    bool sendResponse(uint8_t seqNum, uint16_t commandType, ProtocolConstants::ResponseStatus status) {
        CommandResponse response;
        response.seqNum = seqNum;
        response.commandType = commandType;
        response.status = status;
        return sendResponse(response);
    }

    bool sendResponse(const CommandResponse &response) {
        MutexGuard guard(sendMutex);

        if (!encoder || !transport) {
            LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
            return false;
        }

        const SerializedData frame = encoder->serializeResponse(response);

        if (frame.size == 0) {
            LOG(LogLevel::OP_ERROR, "Failed to serialize response");
            return false;
        }

        return transport->send(frame);
    }

    void listen() {
        MutexGuard guard(listenMutex);

        if (!encoder || !transport) {
            LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
            return;
        }

        transport->accumulate();

        if (transport->hasCompleteFrame()) {
            RawData frame = transport->getFrame();

            if (!ProtocolConstants::isValidFrameType(frame.data[0])) {
                LOG(LogLevel::OP_WARNING, "Received frame with invalid type, discarding");
                transport->releaseFrame();
                return;
            }

            const ProtocolConstants::FrameType frameType =
                    ProtocolConstants::decodeType(frame.data[0]);

            if (frameType == ProtocolConstants::FrameType::COMMAND) {
                PackedCommand packed;
                if (encoder->extractCommandPayload(frame, packed.paramBytes, packed.paramSize, packed.seqNum)) {
                    queue.push(packed);
                } else {
                    LOG(LogLevel::OP_ERROR, "Failed to extract command payload from frame");
                }
            } else if (frameType == ProtocolConstants::FrameType::RESPONSE) {
                CommandResponse response;
                if (encoder->deserializeResponse(frame, response)) {
                    if (response.seqNum != ProtocolConstants::SEQ_NUM_FIRE_AND_FORGET) {
                        pendingAcks.resolve(response.seqNum, response.commandType);
                    }
                    responseQueue.push(response);
                } else {
                    LOG(LogLevel::OP_ERROR, "Failed to deserialize response");
                }
            } else if (frameType == ProtocolConstants::FrameType::TELEMETRY) {
                Telemetry telemetry;
                if (encoder->deserializeTelemetry(frame, telemetry)) {
                    if (telemetryCallback) {
                        telemetryCallback(telemetry, telemetryCallbackContext);
                    } else {
                        LOG(LogLevel::OP_WARNING, "Telemetry received but no callback registered");
                    }
                } else {
                    LOG(LogLevel::OP_ERROR, "Failed to deserialize telemetry");
                }
            } else {
                LOG(LogLevel::OP_WARNING, "Received frame with invalid type, discarding");
            }
            transport->releaseFrame();
        }
    }

    void processCommands() {
        if (!callback) {
            if (!queue.isEmpty()) {
                LOG(LogLevel::OP_WARNING, "Commands queued but no callback registered");
            }
            return;
        }

        PackedCommand packed;
        while (queue.pop(packed)) {
            Command cmd;
            if (!CommandPacker::unpack(packed.paramBytes, packed.paramSize, cmd)) {
                LOG(LogLevel::OP_ERROR, "Failed to unpack command from queue");
                continue;
            }
            callback(cmd, packed.seqNum, callbackContext);
        }
    }

    void processResponses() {
        if (!responseCallback) {
            if (!responseQueue.isEmpty()) {
                LOG(LogLevel::OP_WARNING, "Responses queued but no callback registered");
            }
            return;
        }
        CommandResponse response;
        while (responseQueue.pop(response)) {
            responseCallback(response, responseCallbackContext);
        }
    }

    uint8_t pendingCount() const {
        return queue.size();
    }

    void flushQueue() {
        queue.clear();
    }

    void flushResponseQueue() {
        responseQueue.clear();
    }
};

#endif // SMARTDRIVE_COMMUNICATIONMANAGER_H
