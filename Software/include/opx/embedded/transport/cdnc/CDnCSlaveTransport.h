// CDnCSlaveTransport.h
// ITransport implementation for the CDnC slave side (ATmega328P / AVR).
//
// Physical layer: ISR-driven bit-bang state machine.
//   - ISR fires on every rising CLK edge (18 times per exchange)
//   - Cycle 1:    slave drives DATA LOW (start bit)
//   - Cycles 2-9: slave releases line, reads master TX byte
//   - Cycle 9:    slave drives bit 7 of TX byte during 370µs gap
//   - Cycles 10-17: slave drives bits 6-0, then drives END BIT LOW
//   - Cycle 18:  exchange complete, RX byte pushed to queue
//
// Parser: CRC-scanning, no length field.
//   Accumulates bytes, checks CRC after each byte starting from minimum
//   frame size. Frame complete when CRC8(all_bytes_except_last) == last_byte.
//
// Double-buffer design: parser writes into one slot while TransportManager
// reads from the other. Prevents RX queue starvation when consumer is slow.

#ifndef SMARTDRIVE_CDNCSLAVETRANSPORT_H
#define SMARTDRIVE_CDNCSLAVETRANSPORT_H

#if OPX_CDNC_SLAVE

#include "CDnCConfig.h"
#include "opx/shared/constants/ProtocolConstants.h"
#include "opx/shared/interfaces/ITransport.h"
#include "opx/shared/utils/CRC8.h"
#include "opx/shared/utils/Logger.h"
#include <Arduino.h>

// ── Timing constants
// ────────────────────────────────────────────────────────── Debounce: ignore
// edges arriving too close together (noise rejection) Must be less than
// CLK_HALF_US * 2 = 400µs (minimum valid inter-edge time)
#define CDNC_SLAVE_DEBOUNCE_US 200

// Idle reset: if no clock edge arrives within this window, the current
// partial frame is corrupt — reset bit counter and start fresh
#define CDNC_SLAVE_IDLE_THRESHOLD_US 3000

// Master restart detection: if no exchange for this long, master has
// reset — slave should also reset its state
#define CDNC_SLAVE_RESTART_THRESHOLD_US 100000 // 100ms

// ── GPIO macros
// ─────────────────────────────────────────────────────────────── These are set
// up in beginCDnC() based on dataPin. Direct port manipulation for ISR speed —
// must not use digitalWrite() in the ISR. PD4 = Arduino pin 4 = DATA, PD3 =
// Arduino pin 3 = CLK (INT1)

// Set DATA pin as output (open-drain: write LOW to pull line LOW)
#define CDNC_SLAVE_DATA_OUTPUT_MODE() (DDRD |= (1 << _dataPinBit))
// Set DATA pin as input (Hi-Z: external pull-up takes line HIGH)
#define CDNC_SLAVE_DATA_INPUT_MODE() (DDRD &= ~(1 << _dataPinBit))
// Drive DATA LOW (open-drain assertion)
#define CDNC_SLAVE_DATA_WRITE_LOW() (PORTD &= ~(1 << _dataPinBit))
// Release DATA (open-drain release — external pull-up drives HIGH)
#define CDNC_SLAVE_DATA_WRITE_HIGH() (PORTD |= (1 << _dataPinBit))
// Read DATA pin
#define CDNC_SLAVE_DATA_READ() ((PIND >> _dataPinBit) & 1)

// ── Transport ID
// ──────────────────────────────────────────────────────────────
static constexpr uint8_t CDNC_SLAVE_TRANSPORT_ID = 0x10;

class CDnCSlaveTransport : public ITransport {
public:
  CDnCSlaveTransport() = default;

  // ── Lifecycle ─────────────────────────────────────────────────────────────
  bool begin(uint8_t dataPin, uint8_t clkPin) {
    _dataPin = dataPin;
    _clkPin = clkPin;
    _dataPinBit = digitalPinToBitMask(dataPin) == 0
                      ? dataPin
                      : __builtin_ctz(digitalPinToBitMask(dataPin));

    // DATA: open-drain output, drive LOW (idle state = slave present)
    CDNC_SLAVE_DATA_OUTPUT_MODE();
    CDNC_SLAVE_DATA_WRITE_LOW();

    // CLK: input (master drives CLK)
    pinMode(clkPin, INPUT);

    _instance = this;

    // Attach ISR on rising edge of CLK (INT1 = pin 3)
    attachInterrupt(digitalPinToInterrupt(clkPin), cdncSlaveISR, RISING);

    return true;
  }

  // ── Singleton accessor ────────────────────────────────────────────────────
  static CDnCSlaveTransport *instance() { return _instance; }

  // ── ITransport — TX ───────────────────────────────────────────────────────
  // Push frame bytes into the TX queue for transmission on next exchanges.
  bool send(const SerializedData &data) override {
    for (size_t i = 0; i < data.size; i++) {
      uint16_t next = (_txQueueWrite + 1) & TX_QUEUE_MASK;
      if (next == _txQueueRead) {
        LOG(LogLevel::OP_ERROR, "CDnCSlaveTransport: TX queue overflow");
        return false;
      }
      _txQueue[_txQueueWrite] = data.data[i];
      _txQueueWrite = next;
    }
    return true;
  }

  // ── ITransport — RX ───────────────────────────────────────────────────────
  // Drain the RX queue and feed bytes through the CRC-scanning parser.
  // Double-buffer: continues accumulating even when a completed frame
  // is waiting to be consumed.
  void accumulate() override {
    uint8_t byte;
    while (rxPop(byte)) {
      parseByte(byte);
    }
  }

  bool hasCompleteFrame() const override { return _frameLen[_readFrame] > 0; }

  RawData getFrame() override {
    return RawData{_frameBuf[_readFrame], _frameLen[_readFrame]};
  }

  void releaseFrame() override {
    // Clear read slot and advance to the other buffer.
    // Do NOT call resetParser() here — the parser is already running
    // in the write slot and must not be disturbed.
    _frameLen[_readFrame] = 0;
    _readFrame ^= 1;
  }

  // ── Diagnostics ───────────────────────────────────────────────────────────
  uint32_t lastExchangeMicros() const { return _lastExchangeMicros; }

  uint16_t txQueueUsed() const {
    return (uint16_t)((_txQueueWrite - _txQueueRead) & TX_QUEUE_MASK);
  }

  // ── Master restart detection ──────────────────────────────────────────────
  // Call from CDnCSlaveManager::tick() every loop iteration.
  // If no exchange for RESTART_THRESHOLD, master has reset — reset slave state.
  void checkMasterRestart() {
    if (_lastExchangeMicros == 0)
      return;
    if ((micros() - _lastExchangeMicros) > CDNC_SLAVE_RESTART_THRESHOLD_US) {
      // Reset bit counter and TX state — slave will re-sync on next exchange
      noInterrupts();
      _bitCount = 0;
      _rxByte = 0;
      _txBitsPlaced = 0;
      interrupts();

      // Drive DATA LOW to signal presence to master
      CDNC_SLAVE_DATA_OUTPUT_MODE();
      CDNC_SLAVE_DATA_WRITE_LOW();

      _lastExchangeMicros = 0;
    }
  }

  static CDnCSlaveTransport *_instance;

private:
  // ── TX queue ──────────────────────────────────────────────────────────────
  static constexpr uint8_t TX_QUEUE_SIZE = 64;
  static constexpr uint8_t TX_QUEUE_MASK = TX_QUEUE_SIZE - 1;
  volatile uint8_t _txQueue[TX_QUEUE_SIZE] = {0xFF};
  volatile uint8_t _txQueueRead = 0;
  volatile uint8_t _txQueueWrite = 0;

  // ── RX queue ──────────────────────────────────────────────────────────────
  static constexpr uint8_t RX_QUEUE_SIZE = 64;
  static constexpr uint8_t RX_QUEUE_MASK = RX_QUEUE_SIZE - 1;
  volatile uint8_t _rxQueue[RX_QUEUE_SIZE] = {0};
  volatile uint8_t _rxQueueRead = 0;
  volatile uint8_t _rxQueueWrite = 0;

  // ── ISR state ─────────────────────────────────────────────────────────────
  volatile uint8_t _bitCount = 0;
  volatile uint8_t _rxByte = 0;
  volatile uint8_t _txByte = 0xFF;
  volatile uint8_t _txBitsPlaced = 0;
  volatile bool _exchangeDone = false;
  volatile uint32_t _lastClockMicros = 0;
  volatile uint32_t _lastExchangeMicros = 0;

  // ── Pin config ────────────────────────────────────────────────────────────
  uint8_t _dataPin = 4;
  uint8_t _clkPin = 3;
  uint8_t _dataPinBit = 4; // PD4 = bit 4

  // ── Double-buffer frame parser ────────────────────────────────────────────
  static constexpr uint8_t MAX_FRAME = ProtocolConstants::MAX_FRAME_SIZE;
  uint8_t _frameBuf[2][MAX_FRAME];
  uint8_t _frameLen[2] = {0, 0}; // 0 = slot empty, >0 = frame ready
  uint8_t _writeFrame = 0;
  uint8_t _readFrame = 0;

  // CRC-scanning parser state (for write slot)
  uint8_t _framePos = 0;

  // ── RX queue helpers ──────────────────────────────────────────────────────
  void rxPush(uint8_t b) {
    uint8_t next = (_rxQueueWrite + 1) & RX_QUEUE_MASK;
    if (next != _rxQueueRead) {
      _rxQueue[_rxQueueWrite] = b;
      _rxQueueWrite = next;
    }
  }

  bool rxPop(uint8_t &out) {
    if (_rxQueueRead == _rxQueueWrite)
      return false;
    out = _rxQueue[_rxQueueRead];
    _rxQueueRead = (_rxQueueRead + 1) & RX_QUEUE_MASK;
    return true;
  }

  // ── TX queue helper ───────────────────────────────────────────────────────
  uint8_t txPop() {
    if (_txQueueRead == _txQueueWrite)
      return 0xFF; // NOP = release line
    uint8_t b = _txQueue[_txQueueRead];
    _txQueueRead = (_txQueueRead + 1) & TX_QUEUE_MASK;
    return b;
  }

  // ── CRC-scanning frame parser ─────────────────────────────────────────────
  // No length field — finds frame boundaries by checking CRC after each byte.
  // Double-buffer: when a frame completes, flips write slot and resets state
  // so accumulation continues immediately without waiting for consumer.
  void parseByte(uint8_t byte) {
    // While waiting for a header: discard NOP (0xFF) and idle (0x00) bytes
    if (_framePos == 0) {
      if (byte == 0xFF || byte == 0x00)
        return;
      if (!ProtocolConstants::isValidHeader(byte) ||
          !ProtocolConstants::isValidFrameType(byte)) {
        return; // bad header — stay in WAIT_HEADER state
      }
    }

    // Overflow guard
    if (_framePos >= MAX_FRAME) {
      _framePos = 0; // resync
      return;
    }

    _frameBuf[_writeFrame][_framePos++] = byte;

    // Determine minimum frame size from header byte
    uint8_t minFrame = minFrameSize(_frameBuf[_writeFrame][0]);

    // Try CRC match once we have enough bytes
    if (_framePos >= minFrame) {
      RawData candidate{_frameBuf[_writeFrame], (uint16_t)(_framePos - 1)};
      uint8_t computed = CRC8::compute(candidate);
      if (computed == byte) {
        // Frame complete — store length, flip write slot, reset parser
        _frameLen[_writeFrame] = _framePos;
        _writeFrame ^= 1; // switch to other slot
        _framePos = 0;    // reset for next frame — do NOT touch _readFrame
      }
    }
  }

  static uint8_t minFrameSize(uint8_t header) {
    const ProtocolConstants::FrameType ft =
        ProtocolConstants::decodeType(header);
    switch (ft) {
    case ProtocolConstants::FrameType::COMMAND:
      // header(1) + seqNum(1) + cmdType(2) + CRC(1) = 5
      return 5;
    case ProtocolConstants::FrameType::RESPONSE:
      // header(1) + seqNum(1) + cmdType(2) + status(1) + CRC(1) = 6
      return 6;
    case ProtocolConstants::FrameType::TELEMETRY:
    case ProtocolConstants::FrameType::SETTING:
      // header(1) + id(2) + CRC(1) = 4 minimum
      return 4;
    default:
      return 5;
    }
  }

  // ── ISR ───────────────────────────────────────────────────────────────────
  // Fires on every rising CLK edge.
  // 18 edges per exchange: start bit, 8 TX bits, 8 RX bits, end bit.
  static void cdncSlaveISR() {
    CDnCSlaveTransport *self = _instance;
    if (!self)
      return;

    uint32_t now = micros();

    // Ignore if exchange already complete (shouldn't happen but guard anyway)
    if (self->_bitCount >= 18)
      return;

    // Debounce: ignore edges that arrive too soon (noise)
    if (now - self->_lastClockMicros < CDNC_SLAVE_DEBOUNCE_US)
      return;

    // Idle reset: if too long since last edge, partial frame is corrupted
    if (self->_bitCount > 0 &&
        (now - self->_lastClockMicros) > CDNC_SLAVE_IDLE_THRESHOLD_US) {
      self->_bitCount = 0;
      self->_rxByte = 0;
      self->_txBitsPlaced = 0;
    }

    self->_lastClockMicros = now;
    self->_bitCount++;

    // ── Cycle 1: drive start bit LOW ─────────────────────────────────────
    if (self->_bitCount == 1) {
      CDNC_SLAVE_DATA_OUTPUT_MODE();
      CDNC_SLAVE_DATA_WRITE_LOW();
      return;
    }

    // ── Cycles 2-9: read master TX byte ──────────────────────────────────
    if (self->_bitCount <= 9) {
      if (self->_bitCount == 2) {
        // Release line — let master drive
        CDNC_SLAVE_DATA_INPUT_MODE();
      }

      // Shift in master's bit (MSB first)
      self->_rxByte <<= 1;
      self->_rxByte |= CDNC_SLAVE_DATA_READ();

      if (self->_bitCount == 9) {
        // All 8 master bits received — load TX byte and drive bit 7
        // during the 370µs asymmetric gap before cycle 10 rising edge
        self->_txByte = self->txPop();
        self->_txBitsPlaced = 0;

        // Drive bit 7 NOW (MSB first) so master can sample at cycle 10
        CDNC_SLAVE_DATA_OUTPUT_MODE();
        CDNC_SLAVE_DATA_WRITE_LOW(); // ensure PORT=0 before DDR change
        uint8_t bitIdx = 7;
        if ((self->_txByte >> bitIdx) & 1) {
          CDNC_SLAVE_DATA_WRITE_HIGH();
        } else {
          CDNC_SLAVE_DATA_WRITE_LOW();
        }
        self->_txBitsPlaced = 1;
      }
      return;
    }

    // ── Cycles 10-17: drive TX byte to master ────────────────────────────
    if (self->_bitCount <= 17) {
      if (self->_txBitsPlaced < 8) {
        // Drive next data bit (bit 7 already placed at cycle 9)
        uint8_t bitIdx = 7 - self->_txBitsPlaced;
        if ((self->_txByte >> bitIdx) & 1) {
          CDNC_SLAVE_DATA_WRITE_HIGH();
        } else {
          CDNC_SLAVE_DATA_WRITE_LOW();
        }
        self->_txBitsPlaced++;

        // After last data bit: immediately drive END BIT LOW
        // so master can sample it at cycle 18
        if (self->_txBitsPlaced == 8) {
          CDNC_SLAVE_DATA_WRITE_LOW();
        }
      } else {
        // All data bits sent — hold END BIT LOW
        CDNC_SLAVE_DATA_WRITE_LOW();
      }
      return;
    }

    // ── Cycle 18: exchange complete ───────────────────────────────────────
    if (self->_bitCount == 18) {
      self->rxPush(self->_rxByte); // push received byte to RX queue
      self->_exchangeDone = true;
      self->_lastExchangeMicros = micros();

      // Reset for next exchange — drive DATA LOW (idle = slave present)
      self->_bitCount = 0;
      self->_rxByte = 0;
      self->_txBitsPlaced = 0;

      CDNC_SLAVE_DATA_OUTPUT_MODE();
      CDNC_SLAVE_DATA_WRITE_LOW();
    }
  }
};

#endif // OPX_CDNC_SLAVE
#endif // SMARTDRIVE_CDNCSLAVETRANSPORT_H
