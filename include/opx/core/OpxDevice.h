//
// Created by dunamis on 06/05/2026.
//

#ifndef SMARTDRIVE_OPXDEVICE_H
#define SMARTDRIVE_OPXDEVICE_H

#ifdef ARDUINO
#include "opx/core/platform.h"
#include "opx/core/CommunicationManager.h"
#include "opx/core/TransportManager.h"
#include "opx/core/TelemetryManager.h"
#include "opx/core/TriggerConfig.h"
#include "opx/core/PlatformClock.h"
#include "opx/protocol/BinaryEncoder.h"
#include "opx/types/ProtocolTypes.h"
#include "opx/utils/Logger.h"
#include "opx/interfaces/ITransport.h"

#ifdef ESP32
#include "opx/transport/wifi/EspWiFiTransport.h"
#include "opx/transport/http/EspHttpTransport.h"
#include "opx/mutex/FreeRtosMutex.h"
#endif

#include "opx/transport/serial/ArduinoSerialTransport.h"
#include "opx/mutex/NullMutex.h"

#ifdef CDNC_ENABLED
#include "smartdrive/transport/cdnc/CDnCTransport.h"
#include "smartdrive/transport/cdnc/CDnCManager.h"
#endif

enum class OpxDeviceTransportID : uint8_t {
    OPX_SERIAL = 0x30,
    OPX_WIFI = 0x31,
    OPX_HTTP = 0x32,
};

class OpxDevice {
public:
    using CommandHandler = void (*)(const Command &, const uint8_t &,
                                    uint8_t sourceTransportID, void *context);
    using ResponseHandler = void (*)(const CommandResponse &,
                                     uint8_t sourceTransportID, void *context);
    using TelemetryHandler = void (*)(const Telemetry &,
                                      uint8_t sourceTransportID, void *context);

    OpxDevice();

    ~OpxDevice();

    OpxDevice(const OpxDevice &) = delete;

    OpxDevice &operator=(const OpxDevice &) = delete;

    template<typename SerialType>
    bool beginSerial(SerialType &serial, uint32_t baud);

#ifdef ESP32
bool beginWiFi(uint16_t port, uint32_t stackSize = 4096);
bool beginHttpServer(uint16_t port, uint32_t stackSize = 4096);
bool beginHttpClient(const char *host, uint16_t port);
#endif

#ifdef CDNC_ENABLED
bool beginCDnC();
#endif

void end(OpxDeviceTransportID id);

#ifdef CDNC_ENABLED
void endCDnC();
#endif

void endAll();
void onCommand(CommandHandler handler, void *context = nullptr);
void onResponse(ResponseHandler handler, void *context = nullptr);
void onIncomingTelemetry(TelemetryHandler handler, void *context = nullptr);
void update();

#ifdef CDNC_ENABLED
void exchangeCDnC();
#endif

bool sendCommand(const Command &cmd,
                 uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT);
bool sendResponse(const CommandResponse &response);
bool sendResponse(uint8_t seqNum, uint16_t commandType,
                  ProtocolConstants::ResponseStatus status);
bool sendTelemetry(const Telemetry &telemetry);
bool registerTelemetry(uint16_t sourceID, TriggerConfig trigger);
bool updateTelemetry(uint16_t sourceID, const ValueSource &value);
bool sendTelemetryNow(uint16_t sourceID);
bool setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger);
bool enableTelemetry(uint16_t sourceID);
bool disableTelemetry(uint16_t sourceID);
bool unregisterTelemetry(uint16_t sourceID);
CommunicationManager *comms();

private:
struct TransportSlot {
    ITransport *transport = nullptr; // heap-allocated, owned
    OpxDeviceTransportID id;
    bool active = false;
};

static constexpr uint8_t MAX_DEVICE_TRANSPORTS = 3; // SERIAL, WIFI, HTTP
TransportSlot *findSlot(OpxDeviceTransportID id);
bool slotOccupied(OpxDeviceTransportID id) const;
void ensureCommunicationManager();
void ensureTelemetryManager();
void rewireHandlers();
bool addTransport(ITransport *transport, OpxDeviceTransportID id);
void removeTransport(OpxDeviceTransportID id);

static void commandBridge(const Command &cmd,
                          const uint8_t &seqNum,
                          uint8_t sourceTransportID,
                          void *context);

static void responseBridge(const CommandResponse &response,
                           uint8_t sourceTransportID,
                           void *context);

static void telemetryBridge(const Telemetry &telemetry,
                            uint8_t sourceTransportID,
                            void *context);

BinaryEncoder encoder;
PlatformClock clock;
TransportManager tm;

#ifdef ESP32
// FreeRTOS mutexes for thread safety between the listen task
// and the main loop's processCommands() / processResponses() calls.
FreeRtosMutex sendMutex;
FreeRtosMutex listenMutex;
#else
NullMutex sendMutex;
NullMutex listenMutex;
#endif
CommunicationManager *cm = nullptr;
TelemetryManager *telemetryManager = nullptr;

// Non-CDnC transport slots
TransportSlot slots[MAX_DEVICE_TRANSPORTS];
uint8_t activeSlotCount = 0;

#ifdef ESP32
// FreeRTOS task handles — stored so tasks can be deleted on end()
TaskHandle_t listenTaskHandle = nullptr;
volatile bool listenTaskShouldStop = false;
SemaphoreHandle_t listenTaskDoneSem = nullptr;
friend void opxListenTask(void *param);
void stopListenTask();
#endif

#ifdef CDNC_ENABLED
// Owns all 16 CDnCTransport instances + broadcast transport.
// Constructed on beginCDnC(), nullptr otherwise.
CDnCManager *cdncManager = nullptr;
bool cdncActive = false;
#endif
CommandHandler commandHandler = nullptr;
void *commandHandlerContext = nullptr;
ResponseHandler responseHandler = nullptr;
void *responseHandlerContext = nullptr;
TelemetryHandler telemetryHandler = nullptr;
void *telemetryHandlerContext = nullptr;

};

template<typename SerialType>
bool OpxDevice::beginSerial(SerialType &serial, uint32_t baud) {
    if (slotOccupied(OpxDeviceTransportID::OPX_SERIAL)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: SERIAL slot already occupied. Call end(SERIAL) first.");
        return false;
    }

    // Construct with the concrete SerialType so we can call begin().
    // After begin(), we only need ITransport* — the concrete type is erased.
    auto *transport = new ArduinoSerialTransport<SerialType>(serial, baud);
    transport->begin(); // calls serial.begin(baud) internally

    return addTransport(transport, OpxDeviceTransportID::OPX_SERIAL);
}

#endif
#endif //SMARTDRIVE_OPXDEVICE_H
