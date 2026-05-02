//
// CDnCTransport.h
// ITransport implementation wrapping the CDnC half-duplex parallel bus.
// One instance per slave (index 0–15). Slave index == transport ID.
//

#ifndef SMARTDRIVE_CDNCTRANSPORT_H
#define SMARTDRIVE_CDNCTRANSPORT_H

#include "smartdrive/interfaces/ITransport.h"
#include "smartdrive/constants/ProtocolConstants.h"
#include "smartdrive/utils/Logger.h"
#include "CDnC.h"

// Sentinel value passed to CDnCTransport constructor to create a broadcast
// instance (used during DISCOVERY — send() enqueues to all 16 slave slots).
// A broadcast instance never produces received frames; accumulate() is a no-op.
static constexpr uint8_t CDNC_SLAVE_BROADCAST = 0xFF;

class CDnCTransport : public ITransport {
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    // Normal unicast transport bound to a single slave (index 0–15).
    // Pass CDNC_SLAVE_BROADCAST to create a TX-only broadcast instance.
    explicit CDnCTransport(uint8_t slaveIndex)
        : _slaveIndex(slaveIndex) {}

    // -----------------------------------------------------------------------
    // ITransport interface
    // -----------------------------------------------------------------------

    // Enqueue all bytes of `data` into the CDnC TX ring buffer for this slave.
    // For the broadcast instance, enqueues to all 16 slave slots.
    // Returns false immediately if any cdnc_send_byte() call reports overflow.
    bool send(const SerializedData& data) override {
        if (_slaveIndex == CDNC_SLAVE_BROADCAST) {
            return sendBroadcast(data);
        }

        for (size_t i = 0; i < data.size; i++) {
            if (!cdnc_send_byte(_slaveIndex, data.data[i])) {
                LOG(LogLevel::OP_ERROR, "CDnCTransport: TX queue overflow on slave");
                return false;
            }
        }
        return true;
    }

    // Drain the CDnC RX ring buffer for this slave one byte at a time,
    // feeding each byte through the OPX frame parser state machine.
    // Stops as soon as a complete frame is assembled (_frameReady = true)
    // so the frame is not overwritten before the caller calls releaseFrame().
    // No-op for the broadcast instance.
    void accumulate() override {
        if (_slaveIndex == CDNC_SLAVE_BROADCAST) return;
        if (_frameReady) return;   // hold current frame until released

        uint8_t byte;
        while (cdnc_recv_byte(_slaveIndex, &byte)) {
            if (parseByte(byte)) {
                break;  // frame complete — stop draining until released
            }
        }
    }

    bool hasCompleteFrame() const override {
        return _frameReady;
    }

    // Returns a RawData view into the internal frame buffer.
    // Valid only between hasCompleteFrame() == true and releaseFrame().
    RawData getFrame() override {
        return RawData{ _frameBuf, _framePos };
    }

    // Discard the current frame and reset the parser, ready for the next frame.
    void releaseFrame() override {
        _frameReady = false;
        resetParser();
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    uint8_t slaveIndex() const { return _slaveIndex; }

    bool isBroadcast() const { return _slaveIndex == CDNC_SLAVE_BROADCAST; }

private:
    // -----------------------------------------------------------------------
    // OPX frame parser
    // -----------------------------------------------------------------------
    // Implements a lean state machine that reconstructs OPX frames from the
    // raw byte stream coming out of the CDnC RX ring buffer.
    //
    // Frame layout (from ProtocolConstants):
    //   [0]        HEADER  — (FrameType << 5) | 0x02  (STX_PATTERN in lower 5 bits)
    //   [1]        LENGTH  — payload byte count (not including header, length, or CRC)
    //   [2..N-1]   PAYLOAD — LENGTH bytes
    //   [N]        CRC     — CRC8 over bytes 0..N-1
    //
    // NOP/pad bytes (0x00) are silently discarded while waiting for a header.
    // Invalid headers are also discarded; the parser stays in WAIT_HEADER.
    // -----------------------------------------------------------------------

    enum class ParseState : uint8_t {
        WAIT_HEADER,
        WAIT_LENGTH,
        READING_PAYLOAD,
        READING_CRC
    };

    uint8_t  _slaveIndex;
    ParseState _state    = ParseState::WAIT_HEADER;
    uint8_t  _frameBuf[ProtocolConstants::MAX_FRAME_SIZE] = {};
    uint8_t  _expectedPayloadLen = 0;   // LENGTH field value
    uint8_t  _framePos   = 0;           // bytes written to _frameBuf so far
    bool     _frameReady = false;

    // Feed one byte into the parser. Returns true when a complete frame has
    // been assembled into _frameBuf (caller should stop draining at that point).
    bool parseByte(uint8_t byte) {
        switch (_state) {

        case ParseState::WAIT_HEADER:
            // Silently discard NOP/pad bytes and invalid headers.
            if (byte == ProtocolConstants::NOP_BYTE) return false;
            if (!ProtocolConstants::isValidHeader(byte) ||
                !ProtocolConstants::isValidFrameType(byte)) {
                LOG(LogLevel::OP_WARNING, "CDnCTransport: invalid header byte, discarding");
                return false;
            }
            _framePos = 0;
            _frameBuf[_framePos++] = byte;
            _state = ParseState::WAIT_LENGTH;
            return false;

        case ParseState::WAIT_LENGTH:
            _expectedPayloadLen = byte;
            _frameBuf[_framePos++] = byte;

            // Sanity check: reject obviously oversized length fields before
            // we start buffering payload bytes.
            if (_expectedPayloadLen > ProtocolConstants::MAX_PAYLOAD_SIZE) {
                LOG(LogLevel::OP_WARNING, "CDnCTransport: payload length exceeds max, resyncing");
                resetParser();
                return false;
            }

            if (_expectedPayloadLen == 0) {
                // Zero-length payload: next byte is directly the CRC.
                _state = ParseState::READING_CRC;
            } else {
                _state = ParseState::READING_PAYLOAD;
            }
            return false;

        case ParseState::READING_PAYLOAD:
            _frameBuf[_framePos++] = byte;

            // _framePos currently points one past the last byte written.
            // Payload starts at index 2 (after HEADER + LENGTH).
            if (_framePos == 2 + _expectedPayloadLen) {
                _state = ParseState::READING_CRC;
            }
            return false;

        case ParseState::READING_CRC:
            _frameBuf[_framePos++] = byte;
            _frameReady = true;
            // Stay in READING_CRC state; releaseFrame() resets to WAIT_HEADER.
            return true;    // signal: frame complete
        }

        return false;   // unreachable, silences compiler warning
    }

    void resetParser() {
        _state = ParseState::WAIT_HEADER;
        _framePos = 0;
        _expectedPayloadLen = 0;
    }

    // -----------------------------------------------------------------------
    // Broadcast TX helper
    // -----------------------------------------------------------------------

    bool sendBroadcast(const SerializedData& data) {
        for (uint8_t slave = 0; slave < CDNC_NUM_SLAVES; slave++) {
            for (size_t i = 0; i < data.size; i++) {
                if (!cdnc_send_byte(slave, data.data[i])) {
                    LOG(LogLevel::OP_ERROR, "CDnCTransport: broadcast TX overflow on slave");
                    return false;
                }
            }
        }
        return true;
    }
};

#endif // SMARTDRIVE_CDNCTRANSPORT_H