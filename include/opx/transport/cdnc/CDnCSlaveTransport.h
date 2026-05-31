//
// CDnCSlaveTransport.h
// ITransport implementation for the CDnC slave side.
// Works on ATmega328P and any platform supporting standard Arduino GPIO API.
//
// One instance per slave device. The slave has exactly one data pin and one
// clock pin. The master drives the clock; the slave responds within each
// 18-cycle exchange frame.
//
// ISR CONSTRAINT: attachInterrupt() requires a plain function pointer on AVR.
// CDnCSlaveTransport registers a static ISR that operates on a single global
// state instance. Only one CDnCSlaveTransport may be active at a time.

#pragma once

#ifdef CDNC_SLAVE

#include "opx/interfaces/ITransport.h"
#include "opx/constants/ProtocolConstants.h"
#include "opx/utils/Logger.h"
#include "opx/utils/CRC8.h"
#include "CDnC.h"

// ============================================================================
// //  Platform GPIO abstraction
//  Default: standard Arduino API (ATmega328P verified, CH32V assumed).
//  Override: define CDNC_SLAVE_CUSTOM_GPIO and provide the five macros
//  yourself before including this header.
// ============================================================================
#if !defined(CDNC_SLAVE_CUSTOM_GPIO)

#define CDNC_SLAVE_DATA_WRITE_LOW()  do { \
digitalWrite(CDnCSlaveTransport::instance()->dataPin(), LOW); \
pinMode(CDnCSlaveTransport::instance()->dataPin(), OUTPUT); \
} while(0)

#define CDNC_SLAVE_DATA_WRITE_HIGH() \
pinMode(CDnCSlaveTransport::instance()->dataPin(), INPUT)

#define CDNC_SLAVE_DATA_INPUT_MODE() \
pinMode(CDnCSlaveTransport::instance()->dataPin(), INPUT)

#define CDNC_SLAVE_DATA_READ() \
digitalRead(CDnCSlaveTransport::instance()->dataPin())

#define CDNC_SLAVE_ATTACH_ISR(fn) \
attachInterrupt(digitalPinToInterrupt( \
CDnCSlaveTransport::instance()->clkPin()), fn, RISING)

#endif // !CDNC_SLAVE_CUSTOM_GPIO
// ============================================================================
//  TX Queue
//  Holds OPX frame bytes the slave wants to send back to the master.
//  Drained one byte per exchange cycle by the ISR.
// ============================================================================
#define CDNC_SLAVE_TX_QUEUE_SIZE  64
#define CDNC_SLAVE_TX_QUEUE_MASK  (CDNC_SLAVE_TX_QUEUE_SIZE - 1)
#define CDNC_SLAVE_RX_QUEUE_SIZE 32
#define CDNC_SLAVE_RX_QUEUE_MASK (CDNC_SLAVE_RX_QUEUE_SIZE - 1)

// ============================================================================
//  Protocol timing constants
// ============================================================================
#define CDNC_SLAVE_DEBOUNCE_US          350
#define CDNC_SLAVE_IDLE_THRESHOLD_US   1500
#define CDNC_SLAVE_RESTART_THRESHOLD_US 100000


// ============================================================================
//  CDnCSlaveTransport
// ============================================================================
class CDnCSlaveTransport : public ITransport {
public:
    // Singleton accessor — required because the ISR is a plain static function.
    static CDnCSlaveTransport* instance() { return _instance; }

    CDnCSlaveTransport(uint8_t dataPin, uint8_t clkPin)
    : _dataPin(dataPin), _clkPin(clkPin)
    {
        resetParser();
    }

    // Called by beginCDnC() — configures pins and attaches ISR.
    void begin() {
        _instance = this;  // must be first — macros depend on this

        // Open-drain LOW: clear PORT register first, then set as output
        // This avoids the macro dependency issue during init
        pinMode(_dataPin, OUTPUT);
        digitalWrite(_dataPin, LOW);

        pinMode(_clkPin, INPUT);

        CDNC_SLAVE_ATTACH_ISR(cdncSlaveISR);
    }

    // -------------------------------------------------------------------------
    // ITransport — TX (slave → master)
    // Pushes OPX frame bytes into the TX queue.
    // The ISR drains one byte per exchange cycle.
    // -------------------------------------------------------------------------
    bool send(const SerializedData& data) override {
        for (size_t i = 0; i < data.size; i++) {
            if (!txPush(data.data[i])) {
                LOG(LogLevel::OP_ERROR, "CDnCSlaveTransport: TX queue overflow");
                return false;
            }
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // ITransport — RX (master → slave)
    // Called by TransportManager::listen() every update() cycle.
    // Checks if the ISR has completed an exchange and feeds the received
    // byte into the OPX frame parser.
    // -------------------------------------------------------------------------

    void accumulate() override {
        if (_frameReady) return;

        // Handle exchange completion flag — reset state for next exchange
        noInterrupts();
        bool done = _exchangeDone;
        interrupts();

        if (done) {
            noInterrupts();
            _exchangeDone = false;
            _bitCount     = 0;
            _rxByte       = 0;
            _txBitsPlaced = 0;
            CDNC_SLAVE_DATA_WRITE_LOW();
            uint8_t nextTx = 0x00;
            txPop(&nextTx);
            _txByte = nextTx;
            interrupts();

        }

        // Drain RX queue — parse all available bytes
        uint8_t b;
        while (rxPop(&b)) {
            if (b != 0xFF && b != 0x00) {
                _lastNonZero = b;
                _nonZeroCount++;
            }
            _totalExchanges++;
            if (parseByte(b)) {
                _frameReady = true;
                break;
            }
        }
    }

    bool hasCompleteFrame() const override {
        return _frameReady;
    }

    RawData getFrame() override {
        return RawData{ _frameBuf, _framePos };
    }



    void releaseFrame() override {
        _frameReady = false;
        resetParser();
    }


    // Pin accessors — used by GPIO macros
    uint8_t dataPin() const { return _dataPin; }
    uint8_t clkPin()  const { return _clkPin;  }

    // Called from loop() via update() — detects master restart (>100ms gap)
    void checkMasterRestart() {
        if (_lastExchangeMicros == 0) return;
        if ((micros() - _lastExchangeMicros) > CDNC_SLAVE_RESTART_THRESHOLD_US) {
            _resetFrame();
            _lastExchangeMicros = 0;
            resetParser();
            LOG(LogLevel::OP_WARNING, "CDnCSlaveTransport: master restart detected");
        }
    }

    // Debug accessors
    uint32_t totalExchanges() const { return _totalExchanges; }
    uint32_t nonZeroCount()   const { return _nonZeroCount; }
    uint8_t  lastNonZero()    const { return _lastNonZero; }
    uint8_t  lastBadHeader()  const { return _lastBadHeader; }

    // -------------------------------------------------------------------------
    // Static ISR — runs on every rising clock edge
    // -------------------------------------------------------------------------
    static void cdncSlaveISR() {
        CDnCSlaveTransport* self = _instance;
        if (!self) return;

        uint32_t now = micros();

        if (self->_bitCount >= 18) return;

        // Debounce
        if (now - self->_lastClockMicros < CDNC_SLAVE_DEBOUNCE_US) return;

        // Idle reset — too long between clocks means frame was corrupted
        if (self->_bitCount > 0 &&
            (now - self->_lastClockMicros) > CDNC_SLAVE_IDLE_THRESHOLD_US) {
            self->_bitCount     = 0;
        self->_rxByte       = 0;
        self->_txBitsPlaced = 0;
            }

            self->_lastClockMicros = now;
            self->_bitCount++;

            // Cycle 1: drive start bit LOW, then release for master TX
            if (self->_bitCount == 1) {
                CDNC_SLAVE_DATA_WRITE_LOW();   // start bit — master samples before ISR fires
                return;                         // stay driving LOW, don't release here
            }

            // Cycles 2-9: read master TX byte
            if (self->_bitCount <= 9) {
                if (self->_bitCount == 2) {
                    CDNC_SLAVE_DATA_INPUT_MODE();  // release line NOW — let master drive
                }
                self->_rxByte <<= 1;
                self->_rxByte |= CDNC_SLAVE_DATA_READ();

                if (self->_bitCount == 9) {
                    self->_txBitsPlaced = 0;
                    // Drive first TX bit NOW during the 370µs gap
                    // so master can sample it at the start of cycle 10
                    digitalWrite(self->_dataPin, LOW);  // ensure PORT is 0
                    uint8_t bitIdx = 7;  // MSB first
                    if ((self->_txByte >> bitIdx) & 1) {
                        CDNC_SLAVE_DATA_WRITE_HIGH();
                    } else {
                        CDNC_SLAVE_DATA_WRITE_LOW();
                    }
                    self->_txBitsPlaced = 1;  // already placed bit 7
                }
                return;
            }

            // Cycles 10-17: slave drives TX byte back to master
            if (self->_bitCount <= 17) {
                if (self->_txBitsPlaced < 8) {
                    uint8_t bitIdx = 7 - self->_txBitsPlaced;
                    if ((self->_txByte >> bitIdx) & 1) {
                        CDNC_SLAVE_DATA_WRITE_HIGH();
                    } else {
                        CDNC_SLAVE_DATA_WRITE_LOW();
                    }
                    self->_txBitsPlaced++;
                    // DO NOT drive end bit here — let bit 0 settle
                } else {
                    // _txBitsPlaced == 8, all data bits sent — now drive end bit
                    CDNC_SLAVE_DATA_WRITE_LOW();
                }
                return;
            }

            // Cycle 18: exchange complete
            if (self->_bitCount == 18) {
                 self->rxPush(self->_rxByte);  // push before anything overwrites it
                self->_exchangeDone = true;
                self->_lastExchangeMicros = micros();
            }
    }

private:
    // -------------------------------------------------------------------------
    // TX queue (ISR-safe ring buffer)
    // -------------------------------------------------------------------------
    uint8_t  _txQueue[CDNC_SLAVE_TX_QUEUE_SIZE];
    volatile uint16_t _txQueueWrite = 0;
    volatile uint16_t _txQueueRead  = 0;


    volatile uint8_t  _rxQueue[CDNC_SLAVE_RX_QUEUE_SIZE];
    volatile uint16_t _rxQueueWrite = 0;
    volatile uint16_t _rxQueueRead  = 0;

    bool txPush(uint8_t b) {
        if ((uint16_t)(_txQueueWrite - _txQueueRead) >= CDNC_SLAVE_TX_QUEUE_SIZE)
            return false;
        _txQueue[_txQueueWrite & CDNC_SLAVE_TX_QUEUE_MASK] = b;
        _txQueueWrite++;
        return true;
    }

    bool txPop(uint8_t* out) {
        if (_txQueueRead == _txQueueWrite) return false;
        *out = _txQueue[_txQueueRead & CDNC_SLAVE_TX_QUEUE_MASK];
        _txQueueRead++;
        return true;
    }

    bool rxPush(uint8_t b) {
        if ((uint16_t)(_rxQueueWrite - _rxQueueRead) >= CDNC_SLAVE_RX_QUEUE_SIZE)
            return false;
        _rxQueue[_rxQueueWrite & CDNC_SLAVE_RX_QUEUE_MASK] = b;
        _rxQueueWrite++;
        return true;
    }

    bool rxPop(uint8_t* out) {
        if (_rxQueueRead == _rxQueueWrite) return false;
        *out = _rxQueue[_rxQueueRead & CDNC_SLAVE_RX_QUEUE_MASK];
        _rxQueueRead++;
        return true;
    }

    // -------------------------------------------------------------------------
    // OPX frame parser
    // -------------------------------------------------------------------------

        enum class ParseState : uint8_t {
            WAIT_HEADER,
            ACCUMULATING,  // replaces WAIT_LENGTH, READING_PAYLOAD, READING_CRC
        };


        ParseState _parseState = ParseState::WAIT_HEADER;
        uint8_t    _frameBuf[ProtocolConstants::MAX_FRAME_SIZE];
        uint8_t    _framePos   = 0;
        bool       _frameReady = false;


        bool parseByte(uint8_t byte) {

            switch (_parseState) {

                case ParseState::WAIT_HEADER:
                    if (byte == ProtocolConstants::NOP_BYTE) return false;
                    if (byte == 0xFF) return false;
                    if (!ProtocolConstants::isValidHeader(byte) ||
                        !ProtocolConstants::isValidFrameType(byte)) {
                        _lastBadHeader = byte;
                    return false;
                        }
                        _frameBuf[0] = byte;
                        _framePos    = 1;
                        _parseState  = ParseState::ACCUMULATING;
                        return false;

                case ParseState::ACCUMULATING:
                    if (_framePos >= ProtocolConstants::MAX_FRAME_SIZE) {
                        resetParser();
                        return false;
                    }
                    _frameBuf[_framePos++] = byte;

                    {
                        uint8_t minPayload = 0;
                        ProtocolConstants::FrameType ft = ProtocolConstants::decodeType(_frameBuf[0]);
                        switch (ft) {
                            case ProtocolConstants::FrameType::COMMAND:
                                minPayload = ProtocolConstants::COMMAND_PREAMBLE_SIZE;
                                break;
                            case ProtocolConstants::FrameType::RESPONSE:
                                minPayload = sizeof(CommandResponse);
                                break;
                            case ProtocolConstants::FrameType::TELEMETRY:
                                minPayload = ProtocolConstants::TELEMETRY_PREAMBLE_SIZE;
                                break;
                            case ProtocolConstants::FrameType::SETTING:
                                minPayload = ProtocolConstants::SETTING_PREAMBLE_SIZE;
                                break;
                            default:
                                minPayload = 1;
                                break;
                        }
                        uint8_t minFrame = 1 + minPayload + 1;

                        if (_framePos >= minFrame) {
                            RawData candidate{_frameBuf, _framePos - 1};
                            uint8_t expectedCRC = CRC8::compute(candidate);
                            _lastBadHeader = expectedCRC;  // ← ADD THIS LINE
                            if (expectedCRC == byte) {
                                _frameReady = true;
                                return true;
                            }
                        }
                    }
                    return false;
            }
            return false;
        }

        void resetParser() {
            _parseState = ParseState::WAIT_HEADER;
            _framePos   = 0;
            _frameReady = false;
        }

    //-------------------------------------------------------------------------
    // ISR-shared state (volatile)
    // -------------------------------------------------------------------------
    volatile uint8_t  _bitCount         = 0;
    volatile uint8_t  _rxByte           = 0;
    volatile uint8_t  _txByte           = 0;
    volatile uint8_t  _txBitsPlaced     = 0;
    volatile bool     _exchangeDone     = false;
    volatile uint32_t _lastClockMicros  = 0;
    uint32_t _totalExchanges = 0;
    uint32_t _nonZeroCount   = 0;
    uint8_t  _lastNonZero    = 0;
    uint8_t _lastBadHeader = 0;



    volatile uint32_t _lastExchangeMicros = 0;

    uint8_t _dataPin;
    uint8_t _clkPin;

    void _resetFrame() {
        noInterrupts();
        _bitCount     = 0;
        _rxByte       = 0;
        _txBitsPlaced = 0;
        _exchangeDone = false;
        CDNC_SLAVE_DATA_WRITE_LOW();
        interrupts();
    }

    static CDnCSlaveTransport* _instance;
};

// Static instance pointer definition — goes in OpxDevice.cpp or a dedicated
// CDnCSlaveTransport.cpp to avoid multiple definition errors.
// CDnCSlaveTransport* CDnCSlaveTransport::_instance = nullptr;

#endif // CDNC_SLAVE
