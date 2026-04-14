//
// Created by dunamis on 27/03/2026.
//

#ifndef SMARTDRIVE_TRIGGERCONFIG_H
#define SMARTDRIVE_TRIGGERCONFIG_H

#include "../core/platform.h"

enum class TriggerType : uint8_t {
  ON_CHANGE = 0x00,
  PERIODIC = 0x01,
  ON_REQUEST = 0x02
};

struct TriggerConfig {
  TriggerType type;
  union {
    float threshold; // minimum change to trigger send for ON_CHANGE
    uint32_t intervalMs; // interval for PERIODIC
  };

  static TriggerConfig onChange(const float threshold) {
    TriggerConfig config;
    config.type = TriggerType::ON_CHANGE;
    config.threshold = threshold;
    return config;
  }

  static TriggerConfig periodic(const uint32_t intervalMs) {
    TriggerConfig config;
    config.type = TriggerType::PERIODIC;
    config.intervalMs = intervalMs;
    return config;
  }

  static TriggerConfig onRequest() {
    TriggerConfig config;
    config.type = TriggerType::ON_REQUEST;
    config.intervalMs = 0;
    return config;
  }
};

#endif //SMARTDRIVE_TRIGGERCONFIG_H