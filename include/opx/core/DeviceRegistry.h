//
// Created by dunamis on 27/05/2026.
//

#ifndef OMNIPLEXUS_DEVICEREGISTRY_H
#define OMNIPLEXUS_DEVICEREGISTRY_H

#include "opx/core/platform.h"
#include "opx/core/Config.h"
#include "opx/utils/Logger.h"

class DeviceRegistry {
public:
    using DeviceConnectedCallback = void(*)(uint8_t typeShift, uint8_t transportID, void *context);
    using DeviceDisconnectedCallback = void(*)(uint8_t typeShift, uint8_t transportID, void *context);

private:
    struct DeviceEntry {
        uint8_t typeShift = 0;
        uint8_t transportID = 0;
        bool active = false;
    };

    DeviceEntry entries[MAX_DISCOVERED_DEVICES];
    uint8_t count = 0;

    DeviceConnectedCallback connectedCallback = nullptr;
    DeviceDisconnectedCallback disconnectedCallback = nullptr;
    void *connectedCallbackContext = nullptr;
    void *disconnectedCallbackContext = nullptr;

    int16_t findIndex(uint8_t typeShift) const {
        for (uint8_t i = 0; i < MAX_DISCOVERED_DEVICES; i++) {
            if (entries[i].active && entries[i].typeShift == typeShift) {
                return i;
            }
        }
        return -1;
    }

public:
    DeviceRegistry() {
        for (uint8_t i = 0; i < MAX_DISCOVERED_DEVICES; i++) {
            entries[i].active = false;
        }
    }

    void handleAnnounce(uint8_t typeShift, uint8_t transportID) {
        const int16_t idx = findIndex(typeShift);

        if (idx >= 0) {
            entries[idx].transportID = transportID;
            return;
        }

        if (count >= MAX_DISCOVERED_DEVICES) {
            LOG(LogLevel::OP_ERROR, "DeviceRegistry: Max device limit reached. Cannot register new device.");
            return;
        }

        for (uint8_t i = 0; i < MAX_DISCOVERED_DEVICES; i++) {
            if (!entries[i].active) {
                entries[i].active = true;
                entries[i].typeShift = typeShift;
                entries[i].transportID = transportID;
                count++;
                if (connectedCallback) {
                    connectedCallback(typeShift, transportID, connectedCallbackContext);
                }
                return;
            }
        }
    }

    void handleDisconnect(uint8_t typeShift) {
        const int16_t idx = findIndex(typeShift);
        if (idx < 0) {
            LOG(LogLevel::OP_WARNING, "DeviceRegistry: Received disconnect for unknown device");
            return;
        }

        const uint8_t transportID = entries[idx].transportID;
        entries[idx].active = false;
        entries[idx].transportID = 0;
        entries[idx].typeShift = 0;
        count--;
        if (disconnectedCallback) {
            disconnectedCallback(typeShift, transportID, disconnectedCallbackContext);
        }
    }

    void onDeviceConnected(DeviceConnectedCallback cb, void *context = nullptr) {
        connectedCallback = cb;
        connectedCallbackContext = context;
    }

    void onDeviceDisconnected(DeviceDisconnectedCallback cb, void *context = nullptr) {
        disconnectedCallback = cb;
        disconnectedCallbackContext = context;
    }

    bool isConnected(uint8_t typeShift) const {
        return findIndex(typeShift) >= 0;
    }

    uint8_t transportIDFor(uint8_t typeShift) const {
        const int16_t idx = findIndex(typeShift);
        if (idx < 0) return 0xFF;
        return entries[idx].transportID;
    }

    uint8_t connectedCount() const { return count; }

    void removeByTransport(uint8_t transportID) {
        for (uint8_t i = 0; i < MAX_DISCOVERED_DEVICES; i++) {
            if (entries[i].active && entries[i].transportID == transportID) {
                if (disconnectedCallback) {
                    disconnectedCallback(entries[i].typeShift, transportID, disconnectedCallbackContext);
                }
                entries[i].active = false;
                count--;
            }
        }
    }

    void clear() {
        for (uint8_t i = 0; i < MAX_DISCOVERED_DEVICES; i++) {
            if (entries[i].active && disconnectedCallback) {
                disconnectedCallback(entries[i].typeShift, entries[i].transportID, disconnectedCallbackContext);
            }
            entries[i].active = false;
        }
        count = 0;
    }
};

#endif //OMNIPLEXUS_DEVICEREGISTRY_H
