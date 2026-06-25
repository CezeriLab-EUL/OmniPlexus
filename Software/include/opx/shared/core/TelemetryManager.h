//
// Created by dunamis on 27/03/2026.
//

#pragma once

#include "opx/shared/core/CommunicationManager.h"
#include "opx/shared/core/Config.h"
#include "opx/shared/core/TriggerConfig.h"
#include "opx/shared/interfaces/IPlatformClock.h"
#include "opx/shared/types/RobotData.h"
#include "opx/shared/utils/Logger.h"

class TelemetryManager {
private:
  struct TelemetryEntry {
    Telemetry current;
    ValueSource lastSent;
    TriggerConfig trigger;
    uint32_t lastSentTimeMs;
    bool enabled = true;
    bool active; // this is for if the slot is registered
    bool dirty;  // this is for if the slot has been updated since last send
  };

  TelemetryEntry entries[MAX_TELEMETRY_SOURCES];
  uint8_t count;
  IPlatformClock *clock;
  CommunicationManager *commManager;

  int16_t findIndex(uint16_t sourceID) const {
    for (uint8_t i = 0; i < MAX_TELEMETRY_SOURCES; i++) {
      if (entries[i].active && entries[i].current.sourceID == sourceID) {
        return i;
      }
    }
    return -1;
  }

  bool shouldSend(const TelemetryEntry &entry) const {
    switch (entry.trigger.type) {
    case TriggerType::ON_CHANGE: {
      if (!entry.dirty)
        return false;

      if (entry.current.getType() == ValueType::STRING) {
        return memcmp(entry.current.getData(), entry.lastSent.getData(),
                      entry.current.getDataSize()) != 0;
      }

      float current = 0.0f;
      float last = 0.0f;

      switch (entry.current.getType()) {
      case ValueType::UINT8:
        current = static_cast<uint8_t>(entry.current);
        last = static_cast<uint8_t>(entry.lastSent);
        break;
      case ValueType::INT8:
        current = static_cast<int8_t>(entry.current);
        last = static_cast<int8_t>(entry.lastSent);
        break;
      case ValueType::UINT16:
        current = static_cast<uint16_t>(entry.current);
        last = static_cast<uint16_t>(entry.lastSent);
        break;
      case ValueType::INT16:
        current = static_cast<int16_t>(entry.current);
        last = static_cast<int16_t>(entry.lastSent);
        break;
      case ValueType::UINT32:
        current = static_cast<uint32_t>(entry.current);
        last = static_cast<uint32_t>(entry.lastSent);
        break;
      case ValueType::INT32:
        current = static_cast<int32_t>(entry.current);
        last = static_cast<int32_t>(entry.lastSent);
        break;
      case ValueType::FLOAT:
        current = static_cast<float>(entry.current);
        last = static_cast<float>(entry.lastSent);
        break;
      default:
        return true;
      }
      const float diff = current - last;
      return (diff >= entry.trigger.threshold) ||
             (diff <= -entry.trigger.threshold);
    }

    case TriggerType::PERIODIC: {
      const uint32_t now = clock->millis();
      return (now - entry.lastSentTimeMs) >= entry.trigger.intervalMs;
    }

    case TriggerType::ON_REQUEST:
      return false;

    default:
      return false;
    }
  }

  void doSend(TelemetryEntry &entry) const {
    if (!commManager)
      return;
    commManager->dispatchTelemetry(entry.current);
    entry.lastSent = entry.current;
    entry.lastSentTimeMs = clock->millis();
    entry.dirty = false;
  }

public:
  TelemetryManager(IPlatformClock *clock, CommunicationManager *cm)
      : count(0), clock(clock), commManager(cm) {
    for (uint8_t i = 0; i < MAX_TELEMETRY_SOURCES; i++) {
      entries[i].active = false;
      entries[i].dirty = false;
      entries[i].lastSentTimeMs = 0;
    }
  }

  bool registerSource(uint16_t sourceID, TriggerConfig trigger) {
    if (count >= MAX_TELEMETRY_SOURCES) {
      LOG(LogLevel::OP_ERROR, "Max telemetry sources reached");
      return false;
    }

    if (findIndex(sourceID) >= 0) {
      LOG(LogLevel::OP_WARNING, "Source ID already registered");
      return false;
    }

    for (uint8_t i = 0; i < MAX_TELEMETRY_SOURCES; i++) {
      if (!entries[i].active) {
        entries[i].active = true;
        entries[i].trigger = trigger;
        entries[i].lastSentTimeMs = 0;
        entries[i].dirty = false;
        entries[i].current.sourceID = sourceID;
        count++;
        return true;
      }
    }
    return false;
  }

  void update(uint16_t sourceID, const ValueSource &value) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "Update for unregistered telemetry source");
      return;
    }
    static_cast<ValueSource &>(entries[idx].current) = value;
    entries[idx].dirty = true;
  }

  void send() {
    for (uint8_t i = 0; i < MAX_TELEMETRY_SOURCES; i++) {
      if (!entries[i].active)
        continue;
      if (!entries[i].enabled)
        continue;
      if (shouldSend(entries[i])) {
        doSend(entries[i]);
      }
    }
  }

  const Telemetry *get(uint16_t sourceID) const {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0)
      return nullptr;
    return &entries[idx].current;
  }

  bool sendOne(uint16_t sourceID) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "sendOne: unregistered telemetry source");
      return false;
    }
    doSend(entries[idx]);
    return true;
  }

  void sendAll() {
    for (uint8_t i = 0; i < MAX_TELEMETRY_SOURCES; i++) {
      if (entries[i].active) {
        doSend(entries[i]);
      }
    }
  }

  bool enable(uint16_t sourceID) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "enable: unregistered telemetry source");
      return false;
    }
    entries[idx].enabled = true;
    return true;
  }

  bool setTrigger(uint16_t sourceID, TriggerConfig trigger) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "setTrigger: unregistered telemetry source");
      return false;
    }
    entries[idx].trigger = trigger;
    entries[idx].lastSentTimeMs =
        0; // reset timer to apply new trigger immediately
    return true;
  }

  bool unregisterSource(uint16_t sourceID) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING,
          "unregisterSource: unregistered telemetry source");
      return false;
    }
    entries[idx].active = false;
    entries[idx].dirty = false;
    entries[idx].lastSentTimeMs = 0;
    entries[idx].current.clear();
    entries[idx].lastSent.clear();
    count--;
    return true;
  }

  bool disable(uint16_t sourceID) {
    const int16_t idx = findIndex(sourceID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "disable: unregistered telemetry source");
      return false;
    }
    entries[idx].enabled = false;
    return true;
  }

  uint8_t registeredCount() const { return count; }
};
