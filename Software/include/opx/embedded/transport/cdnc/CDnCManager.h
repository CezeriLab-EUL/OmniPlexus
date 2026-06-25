// CDnCManager.h
// Owns all 16 CDnCTransport instances and handles their registration into
// TransportManager. Also holds the broadcast transport used during DISCOVERY.
//
// Transport ID assignment:
//   0–15                         → per-slave CDnCTransport instances
//   CDNC_TRANSPORT_ID_BROADCAST  → broadcast instance (used for DISCOVERY)

#pragma once

#include "opx/shared/core/Config.h" // IWYU pragma: keep

#if OPX_TARGET_STM32F4xx || OPX_TARGET_STM32F1xx

#include "CDnC.h"
#include "CDnCTransport.h"
#include "opx/shared/core/TransportManager.h"
#include "opx/shared/utils/Logger.h"

static constexpr uint8_t CDNC_TRANSPORT_ID_BROADCAST = 0x20;

class CDnCManager {
public:
  CDnCManager()
      : _broadcastTransport(CDNC_SLAVE_BROADCAST),
        _slaveTransports{
            CDnCTransport(0),  CDnCTransport(1),  CDnCTransport(2),
            CDnCTransport(3),  CDnCTransport(4),  CDnCTransport(5),
            CDnCTransport(6),  CDnCTransport(7),  CDnCTransport(8),
            CDnCTransport(9),  CDnCTransport(10), CDnCTransport(11),
            CDnCTransport(12), CDnCTransport(13), CDnCTransport(14),
            CDnCTransport(15),
        } {}

  bool init(TransportManager *tm) {
    _tm = tm;

    if (!tm->add(&_broadcastTransport, CDNC_TRANSPORT_ID_BROADCAST)) {
      LOG(LogLevel::OP_ERROR,
          "CDnCManager: failed to register broadcast transport");
      return false;
    }

    for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++) {
      if (!tm->add(&_slaveTransports[i], i)) {
        LOG(LogLevel::OP_ERROR,
            "CDnCManager: failed to register slave transport");
        return false;
      }
    }

    return true;
  }

  CDnCTransport &slaveTransport(uint8_t slaveIndex) {
    return _slaveTransports[slaveIndex];
  }

  CDnCTransport &broadcastTransport() { return _broadcastTransport; }

private:
  TransportManager *_tm = nullptr;
  CDnCTransport _broadcastTransport;
  CDnCTransport _slaveTransports[CDNC_MAX_SLAVES];
};

#endif // STM32F4xx || STM32F1xx
