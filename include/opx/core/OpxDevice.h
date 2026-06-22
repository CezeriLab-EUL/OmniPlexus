//
// OpxDevice.h
// OmniPlexus (opx) - Embedded Device Facade
//

#ifndef SMARTDRIVE_OPXDEVICE_H
#define SMARTDRIVE_OPXDEVICE_H

#ifdef ARDUINO
#include "opx/core/platform.h"
#include "opx/core/CommunicationManager.h"
#include "opx/core/TransportManager.h"
#include "opx/core/TelemetryManager.h"
#include "opx/core/SettingsManager.h"
#include "opx/core/DeviceRegistry.h"
#include "opx/core/TriggerConfig.h"
#include "opx/core/PlatformClock.h"
#include "opx/protocol/BinaryEncoder.h"
#include "opx/types/ProtocolTypes.h"
#include "opx/utils/Logger.h"
#include "opx/interfaces/ITransport.h"
#include "opx/interfaces/IPlatformClock.h"

#ifdef ESP32
#include "opx/transport/wifi/EspWiFiTransport.h"
#include "opx/transport/http/EspHttpTransport.h"
#include "opx/mutex/FreeRtosMutex.h"
#endif

#include "opx/transport/serial/ArduinoSerialTransport.h"
#include "opx/mutex/NullMutex.h"

#ifdef CDNC_MASTER
#include "opx/transport/cdnc/CDnCTransport.h"
#include "opx/transport/cdnc/CDnCManager.h"
#include "opx/transport/cdnc/CDnC.h"
#endif

#ifdef CDNC_SLAVE
#include "opx/transport/cdnc/CDnCSlaveTransport.h"
#include "opx/transport/cdnc/CDnCSlaveManager.h"
#endif

enum class OpxDeviceTransportID : uint8_t {
    OPX_SERIAL = 0x30,
    OPX_WIFI   = 0x31,
    OPX_HTTP   = 0x32,
};

class OpxDevice {
public:
    using CommandHandler = void (*)(const Command &, const uint8_t &,
                                    uint8_t sourceTransportID, void *context);
    using ResponseHandler = void (*)(const CommandResponse &,
                                     uint8_t sourceTransportID, void *context);
    using TelemetryHandler = void (*)(const Telemetry &,
                                      uint8_t sourceTransportID, void *context);
    using ConnectionLostHandler = void (*)();

    // ── Protocol-level command hooks ──────────────────────────────────────────
    // Called AFTER the library's own default handling for each protocol command.
    // Use on bridge nodes (e.g. STM32 routing CDnC ↔ Serial) to intercept
    // DISCOVER/ANNOUNCE/HEARTBEAT without blocking the library's protocol logic.
    using ProtocolCommandHook = void(*)(const Command&, uint8_t srcTransportID, void* ctx);

    void onDiscover    (ProtocolCommandHook hook, void* ctx = nullptr);
    void onAnnounce    (ProtocolCommandHook hook, void* ctx = nullptr);
    void onHeartbeat   (ProtocolCommandHook hook, void* ctx = nullptr);
    void onHeartbeatAck(ProtocolCommandHook hook, void* ctx = nullptr);

    #ifdef CDNC_MASTER
    using CdncSlaveCallback = void(*)(uint8_t slaveIndex, void* context);
    void onCdncSlaveConnected   (CdncSlaveCallback cb, void* context = nullptr);
    void onCdncSlaveDisconnected(CdncSlaveCallback cb, void* context = nullptr);
    #endif

    OpxDevice();
    ~OpxDevice();
    OpxDevice(const OpxDevice &) = delete;
    OpxDevice &operator=(const OpxDevice &) = delete;

    template<typename SerialType>
    bool beginSerial(SerialType &serial, uint32_t baud);

    template<typename SerialType>
    bool connectSerial(SerialType &serial, uint32_t baud) {
        return beginSerial(serial, baud);
    }

    void setTypeShift(uint8_t typeShift);
    bool forwardBetween(uint8_t transportA, uint8_t transportB);

    #ifdef ESP32
    bool beginWiFi      (uint16_t port, uint32_t stackSize = 4096);
    bool beginHttpServer(uint16_t port, uint32_t stackSize = 4096);
    bool beginHttpClient(const char *host, uint16_t port);
    bool connectWiFi    (const char *host, uint16_t port,
                         uint8_t maxReconnectAttempts = 5,
                         uint32_t reconnectDelayMs = 2000);
    bool connectHttp    (const char *host, uint16_t port);
    #endif

    #ifdef CDNC_MASTER
    bool     beginCDnC();
    uint16_t exchangeCDnC();
    void     endCDnC();

    bool               cdncSlaveAlive (uint8_t slave);
    uint16_t           cdncAliveMask  ();
    cdnc_slave_state_t cdncSlaveState (uint8_t slave);

    bool     cdncSend     (uint8_t slave, uint8_t byte);
    bool     cdncRecv     (uint8_t slave, uint8_t* out);
    uint8_t  cdncAvailable(uint8_t slave);
    #endif

    #ifdef CDNC_SLAVE
    bool beginCDnC(uint8_t dataPin, uint8_t clkPin);
    void endCDnCSlave();
    #endif

    void end   (OpxDeviceTransportID id);
    void endAll();

    void onCommand          (CommandHandler   handler, void *context = nullptr);
    void onResponse         (ResponseHandler  handler, void *context = nullptr);
    void onIncomingTelemetry(TelemetryHandler handler, void *context = nullptr);
    void onConnectionLost   (ConnectionLostHandler callback);

    void update();

    // Discovery
    void    announce();
    void    discover();
    void    onDeviceConnected   (DeviceRegistry::DeviceConnectedCallback   cb, void *context = nullptr);
    void    onDeviceDisconnected(DeviceRegistry::DeviceDisconnectedCallback cb, void *context = nullptr);
    bool    isDeviceConnected   (uint8_t typeShift) const;
    uint8_t transportIDFor      (uint8_t typeShift) const;

    // Heartbeat
    void setHeartbeatTimeout(uint32_t timeoutMs);

    // Sending
    bool sendCommand (const Command &cmd,
                      uint8_t transportID = ProtocolConstants::TRANSPORT_ID_DEFAULT);
    bool sendResponse(const CommandResponse &response);
    bool sendResponse(uint8_t seqNum, uint16_t commandType,
                      ProtocolConstants::ResponseStatus status);
    bool sendTelemetry(const Telemetry &telemetry);

    // Telemetry management
    bool registerTelemetry  (uint16_t sourceID, TriggerConfig trigger);
    bool updateTelemetry    (uint16_t sourceID, const ValueSource &value);
    bool sendTelemetryNow   (uint16_t sourceID);
    bool setTelemetryTrigger(uint16_t sourceID, TriggerConfig trigger);
    bool enableTelemetry    (uint16_t sourceID);
    bool disableTelemetry   (uint16_t sourceID);
    bool unregisterTelemetry(uint16_t sourceID);

    // Settings management
    bool registerSetting      (uint16_t settingID, ValueType type);
    bool updateSetting        (uint16_t settingID, const ValueSource &value, bool broadcast = false);
    bool attachSettingCallback(uint16_t settingID, SettingsManager::SettingChangedCallback cb, void *context = nullptr);
    void onAnySettingChanged  (SettingsManager::SettingChangedCallback cb, void *context = nullptr);
    void broadcastAllSettings ();
    void broadcastOneSetting  (uint16_t settingID);
    const SettingsData *getSetting(uint16_t settingID) const;

    CommunicationManager *comms();
    TransportManager* transportManager() { return &tm; }
    bool addRawTransport(ITransport* transport, uint8_t id);

private:
    struct TransportSlot {
        ITransport           *transport = nullptr;
        OpxDeviceTransportID  id;
        bool                  active    = false;
    };

    struct ForwardingPair {
        uint8_t transportA = 0;
        uint8_t transportB = 0;
        bool    active     = false;
    };

    ForwardingPair forwardingPairs[MAX_FORWARDING_PAIRS];
    uint8_t        ownTypeShift = 0xFF;

    static constexpr uint8_t MAX_DEVICE_TRANSPORTS = 3;
    TransportSlot *findSlot(OpxDeviceTransportID id);
    bool  slotOccupied(OpxDeviceTransportID id) const;
    void  ensureCommunicationManager();
    void  ensureTelemetryManager();
    void  ensureSettingsManager();
    void  rewireHandlers();
    bool  addTransport   (ITransport *transport, OpxDeviceTransportID id);
    void  removeTransport(OpxDeviceTransportID id);

    static void commandBridge (const Command &cmd, const uint8_t &seqNum,
                               uint8_t sourceTransportID, void *context);
    static void responseBridge(const CommandResponse &response,
                               uint8_t sourceTransportID, void *context);
    static void telemetryBridge(const Telemetry &telemetry,
                                uint8_t sourceTransportID, void *context);
    static void settingBridge  (const SettingsData &setting,
                                uint8_t sourceTransportID, void *context);
    static void forwardBridge  (const TaggedFrame &frame, void *context);
    void        handleForwarding(const TaggedFrame &frame);
    static uint8_t extractTypeShift(const RawData &frame);

    BinaryEncoder  encoder;
    PlatformClock  clock;
    TransportManager tm;
    DeviceRegistry   deviceRegistry;

    uint32_t heartbeatTimeoutMs = 3000;
    uint32_t lastHeartbeatMs    = 0;
    bool     heartbeatReceived  = false;
    bool     connectionLost     = false;

    #ifdef ESP32
    FreeRtosMutex sendMutex;
    FreeRtosMutex listenMutex;
    #else
    NullMutex sendMutex;
    NullMutex listenMutex;
    #endif

    CommunicationManager *cm              = nullptr;
    TelemetryManager     *telemetryManager = nullptr;
    SettingsManager      *settingsManager  = nullptr;

    TransportSlot slots[MAX_DEVICE_TRANSPORTS];
    uint8_t       activeSlotCount = 0;

    #ifdef ESP32
    TaskHandle_t     listenTaskHandle    = nullptr;
    volatile bool    listenTaskShouldStop = false;
    SemaphoreHandle_t listenTaskDoneSem  = nullptr;
    friend void opxListenTask(void *param);
    void stopListenTask();
    #endif

    #ifdef CDNC_MASTER
    CDnCManager *cdncManager        = nullptr;
    bool         cdncActive         = false;
    uint16_t     _cdncPrevAliveMask = 0;

    CdncSlaveCallback _cdncSlaveConnectedCb     = nullptr;
    void*             _cdncSlaveConnectedCtx    = nullptr;
    CdncSlaveCallback _cdncSlaveDisconnectedCb  = nullptr;
    void*             _cdncSlaveDisconnectedCtx = nullptr;
    #endif

    #ifdef CDNC_SLAVE
    CDnCSlaveManager *_cdncSlaveManager = nullptr;
    bool              _cdncSlaveActive  = false;
    #endif

    CommandHandler        commandHandler        = nullptr;
    void                 *commandHandlerContext  = nullptr;
    ResponseHandler       responseHandler        = nullptr;
    void                 *responseHandlerContext  = nullptr;
    TelemetryHandler      telemetryHandler       = nullptr;
    void                 *telemetryHandlerContext = nullptr;
    ConnectionLostHandler connectionLostCallback  = nullptr;

    // Protocol-level command hooks
    ProtocolCommandHook _discoverHook       = nullptr;
    void*               _discoverHookCtx    = nullptr;
    ProtocolCommandHook _announceHook       = nullptr;
    void*               _announceHookCtx    = nullptr;
    ProtocolCommandHook _heartbeatHook      = nullptr;
    void*               _heartbeatHookCtx   = nullptr;
    ProtocolCommandHook _heartbeatAckHook   = nullptr;
    void*               _heartbeatAckHookCtx = nullptr;
};

template<typename SerialType>
bool OpxDevice::beginSerial(SerialType &serial, uint32_t baud) {
    if (slotOccupied(OpxDeviceTransportID::OPX_SERIAL)) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: SERIAL slot already occupied. Call end(SERIAL) first.");
        return false;
    }
    auto *transport = new ArduinoSerialTransport<SerialType>(serial, baud);
    transport->begin();
    return addTransport(transport, OpxDeviceTransportID::OPX_SERIAL);
}

// ── CDNC_MASTER inline definitions ───────────────────────────────────────────
// These must live in the header so the sketch's #define CDNC_MASTER is visible
// at compile time. The Arduino build system compiles library .cpp files
// separately and does not see defines from the sketch.
#ifdef CDNC_MASTER

inline bool OpxDevice::beginCDnC() {
    if (cdncActive) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: CDnC already active.");
        return false;
    }
    ensureCommunicationManager();
    cdnc_init();
    cdncManager = new CDnCManager();
    if (!cdncManager->init(&tm)) {
        LOG(LogLevel::OP_ERROR, "OpxDevice: CDnCManager failed to initialize.");
        delete cdncManager;
        cdncManager = nullptr;
        return false;
    }
    _cdncPrevAliveMask = 0;
    cdncActive = true;
    return true;
}

inline void OpxDevice::endCDnC() {
    if (!cdncActive) return;
    for (uint8_t i = 0; i < CDNC_MAX_SLAVES; i++) tm.remove(i);
    tm.remove(CDNC_TRANSPORT_ID_BROADCAST);
    delete cdncManager;
    cdncManager = nullptr;
    cdncActive  = false;
}

inline uint16_t OpxDevice::exchangeCDnC() {
    if (!cdncActive) return 0;
    uint16_t valid = cdnc_exchange();
    delayMicroseconds(CDNC_GAP_US);
    uint16_t aliveMask = cdnc_alive_mask();
    uint16_t changed   = aliveMask ^ _cdncPrevAliveMask;
    if (changed) {
        for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
            if (!((changed >> s) & 1)) continue;
            bool nowAlive = (aliveMask >> s) & 1;
            if (nowAlive) {
                if (_cdncSlaveConnectedCb)
                    _cdncSlaveConnectedCb(s, _cdncSlaveConnectedCtx);
            } else {
                if (_cdncSlaveDisconnectedCb)
                    _cdncSlaveDisconnectedCb(s, _cdncSlaveDisconnectedCtx);
                deviceRegistry.removeByTransport(s);
            }
        }
        _cdncPrevAliveMask = aliveMask;
    }
    return valid;
}

inline bool               OpxDevice::cdncSlaveAlive (uint8_t s)         { return cdnc_slave_alive(s); }
inline uint16_t           OpxDevice::cdncAliveMask  ()                  { return cdnc_alive_mask(); }
inline cdnc_slave_state_t OpxDevice::cdncSlaveState (uint8_t s)         { return cdnc_slave_state_get(s); }
inline bool               OpxDevice::cdncSend       (uint8_t s, uint8_t b)   { return cdnc_send_byte(s, b); }
inline bool               OpxDevice::cdncRecv       (uint8_t s, uint8_t* o)  { return cdnc_recv_byte(s, o); }
inline uint8_t            OpxDevice::cdncAvailable  (uint8_t s)         { return cdnc_rx_available(s); }

inline void OpxDevice::onCdncSlaveConnected(CdncSlaveCallback cb, void* ctx) {
    _cdncSlaveConnectedCb  = cb;
    _cdncSlaveConnectedCtx = ctx;
}
inline void OpxDevice::onCdncSlaveDisconnected(CdncSlaveCallback cb, void* ctx) {
    _cdncSlaveDisconnectedCb  = cb;
    _cdncSlaveDisconnectedCtx = ctx;
}

#endif // CDNC_MASTER

// ── CDNC_SLAVE inline definitions ────────────────────────────────────────────
#ifdef CDNC_SLAVE

inline bool OpxDevice::beginCDnC(uint8_t dataPin, uint8_t clkPin) {
    if (_cdncSlaveActive) {
        LOG(LogLevel::OP_WARNING, "OpxDevice: CDnC slave already active.");
        return false;
    }
    ensureCommunicationManager();
    _cdncSlaveManager = new CDnCSlaveManager(dataPin, clkPin);
    if (!_cdncSlaveManager->init(&tm)) {
        LOG(LogLevel::OP_ERROR, "OpxDevice: CDnCSlaveManager failed to initialize.");
        delete _cdncSlaveManager;
        _cdncSlaveManager = nullptr;
        return false;
    }
    _cdncSlaveActive = true;
    return true;
}

inline void OpxDevice::endCDnCSlave() {
    if (!_cdncSlaveActive) return;
    tm.remove(CDNC_SLAVE_TRANSPORT_ID);
    delete _cdncSlaveManager;
    _cdncSlaveManager = nullptr;
    _cdncSlaveActive  = false;
}

#endif // CDNC_SLAVE

// ── Protocol hook setters (inline — same reason as above) ────────────────────
inline void OpxDevice::onDiscover    (ProtocolCommandHook h, void* c) { _discoverHook     = h; _discoverHookCtx     = c; }
inline void OpxDevice::onAnnounce    (ProtocolCommandHook h, void* c) { _announceHook     = h; _announceHookCtx     = c; }
inline void OpxDevice::onHeartbeat   (ProtocolCommandHook h, void* c) { _heartbeatHook    = h; _heartbeatHookCtx    = c; }
inline void OpxDevice::onHeartbeatAck(ProtocolCommandHook h, void* c) { _heartbeatAckHook = h; _heartbeatAckHookCtx = c; }

#endif // ARDUINO
#endif // SMARTDRIVE_OPXDEVICE_H
