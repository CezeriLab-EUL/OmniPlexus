//
// Created by dunamis on 25/02/2026.
//

#ifndef SMARTDRIVE_COMMUNICATIONMANAGER_H
#define SMARTDRIVE_COMMUNICATIONMANAGER_H

#include "CommandPacker.h"
#include "opx/shared/core/TransportManager.h"
#include "opx/shared/core/platform.h" // IWYU pragma: keep
#include "opx/shared/interfaces/IEncoder.h"
#include "opx/shared/interfaces/IMutex.h"
#include "opx/shared/types/ProtocolTypes.h"
#include "opx/shared/utils/CommandQueue.h"
#include "opx/shared/utils/Logger.h"
#include "opx/shared/utils/PendingAckQueue.h"
#include "opx/shared/utils/ResponseQueue.h"

class CommunicationManager {
public:
  using CommandCallback = void (*)(const Command &cmd, const uint8_t &seqNum,
                                   uint8_t sourceTransportID, void *context);
  using CommandResponseCallback = void (*)(const CommandResponse &response,
                                           uint8_t sourceTransportID,
                                           void *context);
  using TelemetryCallback = void (*)(const Telemetry &telemetry,
                                     uint8_t sourceTransportID, void *context);
  using SettingCallback = void (*)(const SettingsData &setting,
                                   uint8_t sourceTransportID, void *context);
  using ForwardCallback = void (*)(const TaggedFrame &frame, void *context);

private:
  IEncoder *encoder;
  TransportManager *transportManager;
  uint8_t lastSourceTransportID = ProtocolConstants::TRANSPORT_ID_DEFAULT;
  CommandQueue queue;
  ResponseQueue responseQueue;
  PendingAckQueue pendingAcks;
  uint8_t nextSeqNum = ProtocolConstants::SEQ_NUM_MIN;
  CommandCallback callback = nullptr;
  CommandResponseCallback responseCallback = nullptr;
  TelemetryCallback telemetryCallback = nullptr;
  SettingCallback settingCallback = nullptr;
  ForwardCallback forwardCallback = nullptr;
  void *callbackContext = nullptr;
  void *responseCallbackContext = nullptr;
  void *telemetryCallbackContext = nullptr;
  void *settingCallbackContext = nullptr;
  void *forwardCallbackContext = nullptr;
  IMutex *sendMutex = nullptr;
  IMutex *listenMutex = nullptr;

  struct MutexGuard {
    IMutex *mutex;

    explicit MutexGuard(IMutex *m) : mutex(m) {
      if (mutex)
        mutex->lock();
    }

    ~MutexGuard() {
      if (mutex)
        mutex->unlock();
    }
  };

  static void onFrameReceivedStatic(const TaggedFrame &tagged, void *context) {
    static_cast<CommunicationManager *>(context)->onFrameReceived(tagged);
  }

  void onFrameReceived(const TaggedFrame &tagged) {
    if (forwardCallback) {
      forwardCallback(tagged, forwardCallbackContext);
    }

    const RawData &frame = tagged.frame;

    if (!ProtocolConstants::isValidFrameType(frame.data[0])) {
      LOG(LogLevel::OP_WARNING, "Received frame with invalid type, discarding");
      return;
    }

    const ProtocolConstants::FrameType frameType =
        ProtocolConstants::decodeType(frame.data[0]);

    if (frameType == ProtocolConstants::FrameType::COMMAND) {
      PackedCommand packed;
      if (encoder->extractCommandPayload(frame, packed.paramBytes,
                                         packed.paramSize, packed.seqNum)) {
        packed.sourceTransportID = tagged.transportID;
        queue.push(packed);
      } else {
        LOG(LogLevel::OP_ERROR, "Failed to extract command payload from frame");
      }
    } else if (frameType == ProtocolConstants::FrameType::RESPONSE) {
      CommandResponse response;
      if (encoder->deserializeResponse(frame, response)) {
        if (response.seqNum != ProtocolConstants::SEQ_NUM_FIRE_AND_FORGET) {
          pendingAcks.resolve(response.seqNum, response.commandType);
        }
        responseQueue.push(response, tagged.transportID);
      } else {
        LOG(LogLevel::OP_ERROR, "Failed to deserialize response");
      }
    } else if (frameType == ProtocolConstants::FrameType::TELEMETRY) {
      Telemetry telemetry;
      if (encoder->deserializeTelemetry(frame, telemetry)) {
        if (telemetryCallback) {
          telemetryCallback(telemetry, tagged.transportID,
                            telemetryCallbackContext);
        } else {
          LOG(LogLevel::OP_WARNING,
              "Telemetry received but no callback registered");
        }
      } else {
        LOG(LogLevel::OP_ERROR, "Failed to deserialize telemetry");
      }
    } else if (frameType == ProtocolConstants::FrameType::SETTING) {
      SettingsData setting;
      if (encoder->deserializeSetting(tagged.frame, setting)) {
        if (settingCallback) {
          settingCallback(setting, tagged.transportID, settingCallbackContext);
        } else {
          LOG(LogLevel::OP_WARNING,
              "Setting received but no callback registered");
        }
      } else {
        LOG(LogLevel::OP_ERROR, "Failed to deserialize setting");
      }
    } else {
      LOG(LogLevel::OP_WARNING, "Received frame with invalid type, discarding");
    }
  }

  bool doDispatchCommand(const Command &cmd, uint8_t transportID,
                         bool requiresAck) {
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return false;
    }

    uint8_t resolvedID =
        (transportID == ProtocolConstants::TRANSPORT_ID_DEFAULT)
            ? lastSourceTransportID
            : transportID;

    if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
      resolvedID = transportManager->firstActiveID();
      if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
        LOG(LogLevel::OP_ERROR, "No active transport to dispatch to");
        return false;
      }
    }

    uint8_t seqNum = ProtocolConstants::SEQ_NUM_FIRE_AND_FORGET;

    if (requiresAck) {
      if (pendingAcks.isFull()) {
        LOG(LogLevel::OP_WARNING,
            "Pending ACK queue full, can't dispatch acknowledgeable command");
        return false;
      }
      seqNum = nextSeqNum;
      nextSeqNum = (nextSeqNum >= ProtocolConstants::SEQ_NUM_MAX)
                       ? ProtocolConstants::SEQ_NUM_MIN
                       : nextSeqNum + 1;

      pendingAcks.push({seqNum, cmd.commandType});
    }

    const SerializedData frame = encoder->serializeCommand(cmd, seqNum);

    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize command");
      return false;
    }

    return transportManager->send(frame, resolvedID);
  }

  bool doDispatchTelemetry(const Telemetry &value, uint8_t transportID,
                           bool toAll) {
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return false;
    }

    uint8_t resolvedID =
        (transportID == ProtocolConstants::TRANSPORT_ID_DEFAULT)
            ? lastSourceTransportID
            : transportID;

    if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
      resolvedID = transportManager->firstActiveID();
      if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
        LOG(LogLevel::OP_ERROR, "No active transport to dispatch to");
        return false;
      }
    }

    const SerializedData frame = encoder->serializeTelemetry(value);
    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize telemetry");
      return false;
    }

    if (toAll) {
      return transportManager->sendToAll(frame);
    }

    return transportManager->send(frame, resolvedID);
  }

  bool doDispatchResponse(const CommandResponse &response,
                          uint8_t transportID) {
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return false;
    }

    uint8_t resolvedID =
        (transportID == ProtocolConstants::TRANSPORT_ID_DEFAULT)
            ? lastSourceTransportID
            : transportID;
    if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
      resolvedID = transportManager->firstActiveID();
      if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
        LOG(LogLevel::OP_ERROR, "No active transport to dispatch to");
        return false;
      }
    }

    const SerializedData frame = encoder->serializeResponse(response);

    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize response");
      return false;
    }

    return transportManager->send(frame, resolvedID);
  }

public:
  CommunicationManager(IEncoder *encoder, TransportManager *transportManager,
                       IMutex *sendMutex = nullptr,
                       IMutex *listenMutex = nullptr)
      : encoder(encoder), transportManager(transportManager),
        sendMutex(sendMutex), listenMutex(listenMutex) {
    transportManager->onFrameReceived(onFrameReceivedStatic, this);
  }

  void onCommandReceived(CommandCallback cb, void *context = nullptr) {
    callback = cb;
    callbackContext = context;
  }

  void onResponseReceived(CommandResponseCallback cb, void *context = nullptr) {
    responseCallback = cb;
    responseCallbackContext = context;
  }

  void onTelemetryReceived(TelemetryCallback cb, void *context = nullptr) {
    telemetryCallback = cb;
    telemetryCallbackContext = context;
  }

  void onSettingReceived(SettingCallback cb, void *context = nullptr) {
    settingCallback = cb;
    settingCallbackContext = context;
  }

  void onForwardFrame(ForwardCallback cb, void *context = nullptr) {
    forwardCallback = cb;
    forwardCallbackContext = context;
  }

  bool dispatch(const Command &cmd, bool requiresAck = false) {
    MutexGuard guard(sendMutex);
    return doDispatchCommand(cmd, ProtocolConstants::TRANSPORT_ID_DEFAULT,
                             requiresAck);
  }

  bool dispatch(const Command &cmd, uint8_t transportID,
                bool requiresAck = false) {
    MutexGuard guard(sendMutex);
    return doDispatchCommand(cmd, transportID, requiresAck);
  }

  bool dispatchTelemetry(const Telemetry &value) {
    MutexGuard guard(sendMutex);
    return doDispatchTelemetry(value, ProtocolConstants::TRANSPORT_ID_DEFAULT,
                               false);
  }

  bool dispatchTelemetry(const Telemetry &value, uint8_t transportID) {
    MutexGuard guard(sendMutex);
    return doDispatchTelemetry(value, transportID, false);
  }

  bool dispatchTelemetryToAll(const Telemetry &value) {
    MutexGuard guard(sendMutex);
    return doDispatchTelemetry(value, ProtocolConstants::TRANSPORT_ID_DEFAULT,
                               true);
  }

  bool dispatchCommandToAll(const Command &cmd) {
    MutexGuard guard(sendMutex);
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return false;
    }
    const SerializedData frame = encoder->serializeCommand(
        cmd, ProtocolConstants::SEQ_NUM_FIRE_AND_FORGET);
    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize command");
      return false;
    }
    return transportManager->sendToAll(frame);
  }

  bool dispatchSetting(
      const SettingsData &setting,
      uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT) {
    MutexGuard guard(sendMutex);
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return false;
    }

    uint8_t resolvedID =
        (transportID == ProtocolConstants::TRANSPORT_ID_DEFAULT)
            ? lastSourceTransportID
            : transportID;

    if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
      resolvedID = transportManager->firstActiveID();
      if (resolvedID == ProtocolConstants::TRANSPORT_ID_DEFAULT) {
        LOG(LogLevel::OP_ERROR, "No active transport to dispatch to");
        return false;
      }
    }

    const SerializedData frame = encoder->serializeSetting(setting);
    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize setting");
      return false;
    }

    return transportManager->send(frame, resolvedID);
  }

  bool dispatchSettingToAll(const SettingsData &setting) {
    MutexGuard guard(sendMutex);
    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
    }
    const SerializedData frame = encoder->serializeSetting(setting);
    if (frame.size == 0) {
      LOG(LogLevel::OP_ERROR, "Failed to serialize setting");
      return false;
    }
    return transportManager->sendToAll(frame);
  }

  bool sendResponse(uint8_t seqNum, uint16_t commandType,
                    ProtocolConstants::ResponseStatus status) {
    CommandResponse response;
    response.seqNum = seqNum;
    response.commandType = commandType;
    response.status = status;
    return sendResponse(response);
  }

  bool sendResponse(const CommandResponse &response) {
    MutexGuard guard(sendMutex);
    return doDispatchResponse(response,
                              ProtocolConstants::TRANSPORT_ID_DEFAULT);
  }

  bool sendResponse(const CommandResponse &response, uint8_t transportID) {
    MutexGuard guard(sendMutex);
    return doDispatchResponse(response, transportID);
  }

  void listen() {
    MutexGuard guard(listenMutex);

    if (!encoder || !transportManager) {
      LOG(LogLevel::OP_ERROR, "Communication manager not initialized");
      return;
    }

    transportManager->listen();
  }

  void processCommands() {
    if (!callback) {
      if (!queue.isEmpty()) {
        LOG(LogLevel::OP_WARNING, "Commands queued but no callback registered");
      }
      return;
    }

    PackedCommand packed;
    while (queue.pop(packed)) {
      Command cmd;
      if (!CommandPacker::unpack(packed.paramBytes, packed.paramSize, cmd)) {
        LOG(LogLevel::OP_ERROR, "Failed to unpack command from queue");
        continue;
      }
      lastSourceTransportID = packed.sourceTransportID;
      callback(cmd, packed.seqNum, packed.sourceTransportID, callbackContext);
    }
  }

  void processResponses() {
    if (!responseCallback) {
      if (!responseQueue.isEmpty()) {
        LOG(LogLevel::OP_WARNING,
            "Responses queued but no callback registered");
      }
      return;
    }
    QueuedResponse queued;
    while (responseQueue.pop(queued)) {
      lastSourceTransportID = queued.sourceTransportID;
      responseCallback(queued.response, queued.sourceTransportID,
                       responseCallbackContext);
    }
  }

  uint8_t pendingCount() const { return queue.size(); }

  void flushQueue() { queue.clear(); }

  void flushResponseQueue() { responseQueue.clear(); }
};

#endif // SMARTDRIVE_COMMUNICATIONMANAGER_H
