// CDnCSlaveManager.h
// Owns the CDnCSlaveTransport instance and registers it into TransportManager.
// Calls checkMasterRestart() on every tick() to detect master power cycle.
//
// Usage:
//   CDnCSlaveManager mgr(dataPin, clkPin);
//   mgr.init(&tm);
//   // In loop():
//   mgr.tick();

#ifndef SMARTDRIVE_CDNCSLAVEMANAGER_H
#define SMARTDRIVE_CDNCSLAVEMANAGER_H

#ifdef CDNC_SLAVE

#include "opx/core/TransportManager.h"
#include "opx/utils/Logger.h"
#include "CDnCSlaveTransport.h"

class CDnCSlaveManager {
public:
    CDnCSlaveManager(uint8_t dataPin, uint8_t clkPin)
    : _dataPin(dataPin)
    , _clkPin(clkPin)
    {}

    // Call once after TransportManager is constructed.
    // Initialises the slave transport and registers it.
    bool init(TransportManager* tm) {
        _tm = tm;

        if (!_transport.begin(_dataPin, _clkPin)) {
            LOG(LogLevel::OP_ERROR, "CDnCSlaveManager: failed to initialise transport");
            return false;
        }

        if (!tm->add(&_transport, CDNC_SLAVE_TRANSPORT_ID)) {
            LOG(LogLevel::OP_ERROR, "CDnCSlaveManager: failed to register transport");
            return false;
        }

        return true;
    }

    // Call every loop() iteration.
    // Checks for master restart (no exchanges for >100ms) and resets if needed.
    void tick() {
        _transport.checkMasterRestart();
    }

    CDnCSlaveTransport& transport() { return _transport; }

private:
    uint8_t           _dataPin;
    uint8_t           _clkPin;
    TransportManager* _tm       = nullptr;
    CDnCSlaveTransport _transport;
};

#endif // CDNC_SLAVE
#endif // SMARTDRIVE_CDNCSLAVEMANAGER_H
