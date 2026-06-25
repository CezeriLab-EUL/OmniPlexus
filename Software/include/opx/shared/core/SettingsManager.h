//
// Created by dunamis on 26/05/2026.
//

#ifndef OMNIPLEXUS_SETTINGSMANAGER_H
#define OMNIPLEXUS_SETTINGSMANAGER_H

#include "opx/shared/constants/ProtocolConstants.h"
#include "opx/shared/core/CommunicationManager.h"
#include "opx/shared/core/Config.h"
#include "opx/shared/core/platform.h" // IWYU pragma: keep
#include "opx/shared/types/RobotData.h"
#include "opx/shared/utils/Logger.h"

class SettingsManager {
public:
  using SettingChangedCallback = void (*)(uint16_t settingID,
                                          const ValueSource &newValue,
                                          void *context);

private:
  struct SettingEntry {
    SettingsData current;
    bool active = false;

#ifndef OPX_PLATFORM_AVR
    SettingChangedCallback callback = nullptr;
    void *callbackContext = nullptr;
#endif
  };

  SettingEntry entries[MAX_SETTINGS];
  uint8_t count = 0;
  CommunicationManager *commManager;
  SettingChangedCallback globalCallback = nullptr;
  void *globalCallbackContext = nullptr;

  int16_t findIndex(uint16_t settingID) const {
    for (uint8_t i = 0; i < MAX_SETTINGS; i++) {
      if (entries[i].active && entries[i].current.settingsID == settingID) {
        return i;
      }
    }
    return -1;
  }

  void doDispatch(const SettingsData &setting, uint8_t transportID) const {
    if (!commManager)
      return;
    if (transportID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
      commManager->dispatchSetting(setting);
    } else {
      commManager->dispatchSetting(setting, transportID);
    }
  }

  void fireCallbacks(uint16_t settingID, const ValueSource &newValue,
                     int16_t idx) {
#ifndef OPX_PLATFORM_AVR
    if (entries[idx].callback) {
      entries[idx].callback(settingID, newValue, entries[idx].callbackContext);
    }
#endif
    if (globalCallback) {
      globalCallback(settingID, newValue, globalCallbackContext);
    }
  }

public:
  explicit SettingsManager(CommunicationManager *cm) : commManager(cm) {
    for (uint8_t i = 0; i < MAX_SETTINGS; i++) {
      entries[i].active = false;
#ifndef OPX_PLATFORM_AVR
      entries[i].callback = nullptr;
      entries[i].callbackContext = nullptr;
#endif
    }
  }

  bool registerSetting(uint16_t settingID, ValueType type) {
    if (count >= MAX_SETTINGS) {
      LOG(LogLevel::OP_ERROR, "Max settings reached");
      return false;
    }

    if (findIndex(settingID) >= 0) {
      LOG(LogLevel::OP_WARNING, "Setting ID already registered");
      return false;
    }

    for (uint8_t i = 0; i < MAX_SETTINGS; i++) {
      if (!entries[i].active) {
        entries[i].active = true;
        entries[i].current.settingsID = settingID;
        entries[i].current.initDefault(
            type); // sets type and zero-initializes data
        count++;
        return true;
      }
    }
    return false;
  }

  void broadcastAll() {
    if (!commManager)
      return;
    for (uint8_t i = 0; i < MAX_SETTINGS; i++) {
      if (!entries[i].active)
        continue;
      commManager->dispatchSettingToAll(entries[i].current);
    }
  }

  void broadcastOne(uint16_t settingID) {
    if (!commManager)
      return;
    const int16_t idx = findIndex(settingID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "broadcastOne: unregistered setting");
      return;
    }
    commManager->dispatchSettingToAll(entries[idx].current);
  }

  bool update(uint16_t settingID, const ValueSource &value,
              bool broadcast = false) {
    const int16_t idx = findIndex(settingID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "update: unregistered setting");
      return false;
    }

    static_cast<ValueSource &>(entries[idx].current) = value;

    if (broadcast) {
      if (!commManager)
        return false;
      commManager->dispatchSettingToAll(entries[idx].current);
    }

    return true;
  }

  bool handleCommand(const Command &cmd, uint8_t transportID) {
    const uint16_t cmdType = cmd.commandType;

    if (cmdType == ProtocolConstants::GET_ALL_SETTINGS_COMMAND) {
      for (uint8_t i = 0; i < MAX_SETTINGS; i++) {
        if (!entries[i].active)
          continue;
        doDispatch(entries[i].current, transportID);
      }
      return true;
    }

    const uint8_t localID = cmdType & 0xFF;
    const uint8_t category = (cmdType >> 8) & 0x07;
    const uint8_t typeShift = (cmdType >> 11) & 0x1F;

    const bool isSettingGet = (category == 0x2);
    const bool isSettingSet = (category == 0x3);

    if (!isSettingGet && !isSettingSet) {
      LOG(LogLevel::OP_WARNING, "handleCommand: invalid command type");
      return false;
    }

    const uint16_t settingID =
        static_cast<uint16_t>((typeShift << 8) | localID);

    const int16_t idx = findIndex(settingID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "handleCommand: unregistered setting ID");
      return false;
    }

    if (isSettingGet) {
      doDispatch(entries[idx].current, transportID);
      return true;
    }

    static_cast<ValueSource &>(entries[idx].current) = cmd.params[0];
    doDispatch(entries[idx].current, transportID);
    fireCallbacks(settingID, entries[idx].current, idx);
    return true;
  }

  const SettingsData *get(uint16_t settingID) const {
    const int16_t idx = findIndex(settingID);
    if (idx < 0)
      return nullptr;
    return &entries[idx].current;
  }

#ifndef OPX_PLATFORM_AVR
  bool attachCallback(uint16_t settingID, SettingChangedCallback cb,
                      void *context = nullptr) {
    const int16_t idx = findIndex(settingID);
    if (idx < 0) {
      LOG(LogLevel::OP_WARNING, "attachCallback: unregistered setting");
      return false;
    }
    entries[idx].callback = cb;
    entries[idx].callbackContext = context;
    return true;
  }
#endif
  void onAnySettingChanged(SettingChangedCallback cb, void *context = nullptr) {
    globalCallback = cb;
    globalCallbackContext = context;
  }

  uint8_t registeredCount() const { return count; }
};
#endif // OMNIPLEXUS_SETTINGSMANAGER_H
