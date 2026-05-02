//
// CDnCManager.h
// Owns all 16 CDnCTransport instances and handles their registration into
// TransportManager. Also holds the broadcast transport used during DISCOVERY.
//
// Usage (in firmware setup):
//
//   CDnCManager cdncManager;
//   cdncManager.init(&transportManager);
//
//   // All 16 slave transports are now registered (IDs 0–15).
//   // Broadcast transport is registered at CDNC_TRANSPORT_ID_BROADCAST.
//
//   // After discovery, query which slaves are online:
//   cdnc_alive_mask();  // use CDnC API directly
//   // Alive slaves are already registered and will produce frames when active.
//

#ifndef SMARTDRIVE_CDNCMANAGER_H
#define SMARTDRIVE_CDNCMANAGER_H

#include "smartdrive/core/TransportManager.h"
#include "smartdrive/utils/Logger.h"
#include "CDnCTransport.h"
#include "CDnC.h"

// Transport ID assigned to the broadcast instance in TransportManager.
// Chosen above the 16 slave IDs (0–15) and below the UART/ESP range.
static constexpr uint8_t CDNC_TRANSPORT_ID_BROADCAST = 0x20;

class CDnCManager {
public:
    CDnCManager() : _broadcastTransport(CDNC_SLAVE_BROADCAST) {
        for (uint8_t i = 0; i < CDNC_NUM_SLAVES; i++) {
            // placement: each element constructs in-place with its slave index
            new (&_slaveTransports[i]) CDnCTransport(i);
        }
    }

    // Call once during firmware setup, after cdnc_init() has been called.
    // Registers all 16 slave transports (IDs 0–15) and the broadcast transport.
    bool init(TransportManager* tm) {
        _tm = tm;

        // Register broadcast transport first (used by IndicatorBoardController::discovery())
        if (!tm->add(&_broadcastTransport, CDNC_TRANSPORT_ID_BROADCAST)) {
            LOG(LogLevel::OP_ERROR, "CDnCManager: failed to register broadcast transport");
            return false;
        }

        // Register all 16 slave transports upfront.
        // Slaves that are offline simply never produce complete frames —
        // accumulate() finds nothing in their RX buffers and hasCompleteFrame()
        // stays false. No special handling needed for absent slaves.
        for (uint8_t i = 0; i < CDNC_NUM_SLAVES; i++) {
            if (!tm->add(&_slaveTransports[i], i)) {
                LOG(LogLevel::OP_ERROR, "CDnCManager: failed to register slave transport");
                return false;
            }
        }

        return true;
    }

    // Convenience: returns the transport for a specific slave without going
    // through TransportManager. Useful for diagnostics or direct access.
    CDnCTransport& slaveTransport(uint8_t slaveIndex) {
        return _slaveTransports[slaveIndex];
    }

    CDnCTransport& broadcastTransport() {
        return _broadcastTransport;
    }

private:
    // All 16 slave transports stored inline — no heap allocation.
    // CDnCTransport is small: one uint8_t + one enum byte + one 67-byte buffer
    // + two bookkeeping bytes + one bool = ~72 bytes each → ~1152 bytes total.
    alignas(CDnCTransport) uint8_t _slaveStorage[CDNC_NUM_SLAVES * sizeof(CDnCTransport)];
    CDnCTransport* _slaveTransports = reinterpret_cast<CDnCTransport*>(_slaveStorage);

    CDnCTransport _broadcastTransport;
    TransportManager* _tm = nullptr;
};

#endif // SMARTDRIVE_CDNCMANAGER_H