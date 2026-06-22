//
// Created by dunamis on 01/05/2026.
//

#ifndef SMARTDRIVE_TRANSPORTMANAGER_H
#define SMARTDRIVE_TRANSPORTMANAGER_H

#include "opx/core/Config.h"
#include "opx/core/platform.h"
#include "opx/interfaces/ITransport.h"
#include "opx/utils/Logger.h"

struct TaggedFrame {
    RawData frame;
    uint8_t transportID;
};

using TaggedFrameCallback = void (*)(const TaggedFrame &frame, void *context);

class TransportManager {
private:
    struct TransportEntry {
        ITransport *transport = nullptr;
        uint8_t id = 0;
        bool active = false;
    };

    TransportEntry entries[MAX_TRANSPORTS];
    uint8_t count = 0;
    TaggedFrameCallback frameCallback = nullptr;
    void *frameCallbackContext = nullptr;

    TransportEntry* findEntry(uint8_t id) {
        for (uint8_t i = 0; i < MAX_TRANSPORTS; i++) {
            if (entries[i].active && entries[i].id == id) {
                return &entries[i];
            }
        }
        return nullptr;
    }

public:
    TransportManager() {
        for (uint8_t i = 0; i < MAX_TRANSPORTS; i++) {
            entries[i].active = false;
        }
    }

    void onFrameReceived(TaggedFrameCallback cb, void *context = nullptr) {
        frameCallback = cb;
        frameCallbackContext = context;
    }

    uint8_t firstActiveID() const {
        for (uint8_t i = 0; i < MAX_TRANSPORTS; i++) {
            if (entries[i].active) return entries[i].id;
        }
        return ProtocolConstants::TRANSPORT_ID_DEFAULT;
    }

    bool add(ITransport *transport, uint8_t id) {
        if (count >= MAX_TRANSPORTS) {
            LOG(LogLevel::OP_ERROR, "TransportManager: max transports reached");
            return false;
        }

        if (findEntry(id) != nullptr) {
            LOG(LogLevel::OP_ERROR, "TransportManager: transport ID already in use");
            return false;
        }

        for (uint8_t i=0; i<MAX_TRANSPORTS; i++) {
            if (!entries[i].active) {
                entries[i].transport = transport;
                entries[i].id = id;
                entries[i].active = true;
                count++;
                return true;
            }
        }

        return false;
    }

    bool remove(uint8_t id) {
        for (uint8_t i=0; i<MAX_TRANSPORTS; i++) {
            if (entries[i].active && entries[i].id == id) {
                entries[i].active = false;
                entries[i].transport = nullptr;
                count--;
                return true;
            }
        }
        LOG(LogLevel::OP_WARNING, "TransportManager: transport ID not found for removal");
        return false;
    }

    void listen() {
        if (!frameCallback) {
            LOG(LogLevel::OP_WARNING, "TransportManager: no frame callback registered");
            return;
        }

        for (uint8_t i=0; i<MAX_TRANSPORTS; i++) {
            if (!entries[i].active) continue;
            ITransport* transport = entries[i].transport;
            transport->accumulate();
            if (transport->hasCompleteFrame()) {
                TaggedFrame tagged;
                tagged.frame = transport->getFrame();
                tagged.transportID = entries[i].id;
                frameCallback(tagged, frameCallbackContext);
                transport->releaseFrame();
            }
        }
    }

    bool send(const SerializedData& data, uint8_t transportID) {
        TransportEntry* entry = findEntry(transportID);
        if (entry == nullptr) {
            LOG(LogLevel::OP_ERROR, "TransportManager: transport ID not found for send");
            return false;
        }
        return entry->transport->send(data);
    }

    bool sendToAll(const SerializedData& data) {
        bool allSucceeded = true;
        for (uint8_t i=0; i<MAX_TRANSPORTS; i++) {
            if (!entries[i].active) continue;
            if (!entries[i].transport->send(data)) {
                allSucceeded = false;
            }
        }
        return allSucceeded;
    }

    ITransport* get(uint8_t id) {
        TransportEntry* entry = findEntry(id);
        if (entry == nullptr) return nullptr;
        return entry->transport;
    }

    uint8_t registeredCount() const { return count; }
};

#endif //SMARTDRIVE_TRANSPORTMANAGER_H