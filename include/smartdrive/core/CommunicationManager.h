//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_COMMUNICATIONMANAGER_H
#define SMARTDRIVE_COMMUNICATIONMANAGER_H

#include "platform.h"
#include "../interfaces/IEncoder.h"
#include "../interfaces/ITransport.h"
#include "../types/ProtocolTypes.h"
#include "../utils/CommandQueue.h"
#include "../utils/Logger.h"

class CommunicationManager {
public:
    using CommandCallback = void (*)(const Command& cmd, void* context);
private:
    IEncoder* encoder;
    ITransport* transport;
    CommandQueue queue;
    CommandCallback callback = nullptr;
    void* callbackContext = nullptr;
public:
    CommunicationManager(IEncoder* encoder, ITransport* transport): encoder(encoder), transport(transport) {}

    void onCommandReceived(CommandCallback cb, void* context = nullptr) {
        callback = cb;
        callbackContext = context;
    }

    bool dispatch(const Command& cmd) {
        if (!encoder || !transport) {
            LOG(LogLevel::ERROR, "Communication manager not initialized");
            return false;
        }

        const SerializedData frame = encoder->serializeCommand(cmd);

        if (frame.size == 0) {
            LOG(LogLevel::ERROR, "Failed to serialize command");
            return false;
        }

        return transport->send(frame);
    }

    void listen() {
        if (!encoder || !transport) {
            LOG(LogLevel::ERROR, "Communication manager not initialized");
            return;
        }

        transport->accumulate();

        if (transport->hasCompleteFrame()) {
            RawData frame = transport->getFrame();

            Command cmd;
            if (encoder->deserializeCommand(frame, cmd)) {
                queue.push(cmd);
            }else {
                LOG(LogLevel::ERROR, "Failed to deserialize command");
            }
        }

        drainQueue();
    }

    uint8_t pendinfCount() const {
        return queue.size();
    }

    void flushQueue() {
        queue.clear();
    }

private:
    void drainQueue() {
        if (!callback) {
            if (!queue.isEmpty()) {
                LOG(LogLevel::WARNING, "Commands queued but no callnback registered");
            }
            return;
        }

        Command cmd;
        while (queue.pop(cmd)) {
            callback(cmd, callbackContext);
        }
    }
};

#endif //SMARTDRIVE_COMMUNICATIONMANAGER_H