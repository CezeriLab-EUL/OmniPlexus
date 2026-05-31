//
// CDnCSlaveManager.h
// Owns the single CDnCSlaveTransport instance and registers it with
// TransportManager. On the slave side there is exactly one transport —
// the single data line connecting this slave to the master.
//
// Transport ID used: CDNC_SLAVE_TRANSPORT_ID (0x00 by default).
// This matches the master's transport ID for this slave slot.

#pragma once

#ifdef CDNC_SLAVE

#include "opx/core/TransportManager.h"
#include "opx/utils/Logger.h"
#include "CDnCSlaveTransport.h"

static constexpr uint8_t CDNC_SLAVE_TRANSPORT_ID = 0x00;

class CDnCSlaveManager {
public:
    CDnCSlaveManager(uint8_t dataPin, uint8_t clkPin)
    : _transport(dataPin, clkPin)
    {}

    bool init(TransportManager* tm) {
        _tm = tm;

        _transport.begin();  // configure pins, attach ISR

        if (!tm->add(&_transport, CDNC_SLAVE_TRANSPORT_ID)) {
            LOG(LogLevel::OP_ERROR,
                "CDnCSlaveManager: failed to register transport");
            return false;
        }

        return true;
    }

    CDnCSlaveTransport& transport() { return _transport; }

    // Call from OpxDevice::update() to check for master restart
    void tick() {
        _transport.checkMasterRestart();
    }

private:
    TransportManager*  _tm = nullptr;
    CDnCSlaveTransport _transport;
};

#endif // CDNC_SLAVE
