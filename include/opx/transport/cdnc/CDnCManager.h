//
// CDnCManager.h
// Owns all 16 CDnCTransport instances and handles their registration into
// TransportManager. Also holds the broadcast transport used during DISCOVERY.
//
// Transport ID assignment:
//   0–15                    → per-slave CDnCTransport instances
//   CDNC_TRANSPORT_ID_BROADCAST → broadcast instance (used for DISCOVERY)
//
// All 16 slave transports are registered upfront regardless of whether the
// slave is currently alive. An offline slave's CDnCTransport simply never
// produces a complete frame — accumulate() finds nothing in its RX buffer
// and hasCompleteFrame() stays false. No special handling needed.
//
// Main firmware loop pattern:
//
//   void loop() {
//       cdnc_exchange();                  // drives the physical bus
//       delayMicroseconds(GAP_US);
//       comms.listen();                   // drains RX buffers, surfaces frames
//       comms.processCommands();
//       comms.processResponses();
//   }

#ifndef SMARTDRIVE_CDNCMANAGER_H
#define SMARTDRIVE_CDNCMANAGER_H

#ifdef STM32F4xx

#include "opx/core/TransportManager.h"
#include "opx/utils/Logger.h"
#include "CDnCTransport.h"
#include "CDnC.h"

// Transport ID assigned to the broadcast instance in TransportManager.
// Chosen above the 16 slave IDs (0–15). Must not collide with UART/ESP IDs.
static constexpr uint8_t CDNC_TRANSPORT_ID_BROADCAST = 0x20;

class CDnCManager {
public:
    CDnCManager()
        : _broadcastTransport(CDNC_SLAVE_BROADCAST)
        , _slaveTransports{
            CDnCTransport(0),  CDnCTransport(1),  CDnCTransport(2),  CDnCTransport(3),
            CDnCTransport(4),  CDnCTransport(5),  CDnCTransport(6),  CDnCTransport(7),
            CDnCTransport(8),  CDnCTransport(9),  CDnCTransport(10), CDnCTransport(11),
            CDnCTransport(12), CDnCTransport(13), CDnCTransport(14), CDnCTransport(15),
          }
    {}

    // Initialisation
    // Call once after cdnc_init() has been called.
    // Registers the broadcast transport and all 16 slave transports.
    // Returns false if TransportManager rejects any registration
    // (e.g. MAX_TRANSPORTS exceeded — check Config.h).
    bool init(TransportManager* tm) {
        _tm = tm;

        if (!tm->add(&_broadcastTransport, CDNC_TRANSPORT_ID_BROADCAST)) {
            LOG(LogLevel::OP_ERROR, "CDnCManager: failed to register broadcast transport");
            return false;
        }

        for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++) {
            if (!tm->add(&_slaveTransports[i], i)) {
                LOG(LogLevel::OP_ERROR, "CDnCManager: failed to register slave transport");
                return false;
            }
        }

        return true;
    }

    // Accessors — for diagnostics or direct access without going via TM
     CDnCTransport& slaveTransport(uint8_t slaveIndex) {
        return _slaveTransports[slaveIndex];
    }

    CDnCTransport& broadcastTransport() {
        return _broadcastTransport;
    }

private:
    TransportManager* _tm = nullptr;
    CDnCTransport _broadcastTransport;
    CDnCTransport _slaveTransports[CDNC_MAX_SLAVES];
};

#endif // STM32F4xx

#endif // SMARTDRIVE_CDNCMANAGER_H