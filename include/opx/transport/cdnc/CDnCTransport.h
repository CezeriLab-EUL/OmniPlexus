//
// CDnCTransport.h
// ITransport implementation wrapping the CDnC half-duplex parallel bus.
// One instance per slave (index 0–15). Slave index == transport ID.
//
// NOTE: cdnc_exchange() must be called from the main firmware loop separately.
// CDnCTransport::accumulate() only drains the software RX ring buffers;
// it does not drive the physical bus.

#ifndef SMARTDRIVE_CDNCTRANSPORT_H
#define SMARTDRIVE_CDNCTRANSPORT_H

#ifdef CDNC_MASTER

#include "opx/interfaces/ITransport.h"
#include "opx/constants/ProtocolConstants.h"
#include "opx/utils/Logger.h"
#include "opx/utils/CRC8.h"
#include "CDnC.h"

// Sentinel: pass to CDnCTransport constructor to create a TX-only broadcast
// instance. accumulate() / hasCompleteFrame() / getFrame() are no-ops.

class CDnCTransport : public ITransport {
public:
    explicit CDnCTransport(uint8_t slaveIndex)
    : _slaveIndex(slaveIndex)
    {
        resetParser();
    }

    // ITransport — TX
    // Enqueue all bytes of `data` into the CDnC TX ring buffer for this slave.
    // For the broadcast instance, enqueues identically to all 16 slave slots.
    //
    // Returns false immediately on the first cdnc_send_byte() overflow.
    // Partial enqueue on overflow is acceptable: the OPX framing layer will
    // detect the truncated frame via CRC and header re-sync on the slave side.
    bool send(const SerializedData& data) override {
        if (_slaveIndex == CDNC_SLAVE_BROADCAST) {
            return sendBroadcast(data);
        }
        uint32_t slotsNeeded = (uint32_t)data.size * 8;
        uint32_t w = cdnc_write_ptr(_slaveIndex);
        uint32_t r = cdnc_read_ptr();
        if ((w - r) + slotsNeeded > CDNC_TX_BUF_SIZE) {
            LOG(LogLevel::OP_ERROR, "CDnCTransport: TX buffer would overflow — increase CDNC_TX_BUF_SIZE or slow command rate");
            return false;
        }

        
        for (size_t i = 0; i < data.size; i++) {
            if (!cdnc_send_byte(_slaveIndex, data.data[i])) {
                LOG(LogLevel::OP_ERROR, "CDnCTransport: TX queue overflow");
                return false;
            }
        }
        return true;
    }

    // ITransport — RX
    // Drain the CDnC RX ring buffer for this slave byte-by-byte, feeding each
    // byte through the OPX frame parser state machine.
    //
    // Stops as soon as a complete frame is assembled so the frame buffer is
    // not overwritten before the caller calls releaseFrame(). This satisfies
    // TransportManager::listen()'s single-frame-per-cycle contract.
    //
    // No-op for the broadcast instance (it never receives).
    void accumulate() override {
        if (_slaveIndex == CDNC_SLAVE_BROADCAST) return;
        if (_frameReady) return;

        uint8_t byte;
        while (cdnc_recv_byte(_slaveIndex, &byte)) {
            if (parseByte(byte)) {
                break;
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

    // Discard the current frame and reset the parser for the next frame.
    void releaseFrame() override {
        _frameReady = false;
        resetParser();
    }

    // Accessors
    uint8_t slaveIndex()  const { return _slaveIndex; }
    bool    isBroadcast() const { return _slaveIndex == CDNC_SLAVE_BROADCAST; }


private:
     // OPX frame parser state machine
     // Reconstructs OPX frames from the raw byte stream in the CDnC RX buffer.

    // Discard rules while in WAIT_HEADER:
    //   • 0x00 NOP/pad bytes  — silently dropped (cdnc_post_exchange_pad output)
    //   • Invalid header byte — logged and dropped; parser stays in WAIT_HEADER
    //


    uint8_t    _slaveIndex;
    uint8_t    _lastBadHeader = 0;

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

    // Broadcast TX helper
    bool sendBroadcast(const SerializedData& data) {
        for (uint8_t slave = 0; slave < CDNC_MAX_SLAVES; slave++) {
            for (size_t i = 0; i < data.size; i++) {
                if (!cdnc_send_byte(slave, data.data[i])) {
                    LOG(LogLevel::OP_ERROR, "CDnCTransport: broadcast TX overflow");
                    return false;
                }
            }
        }
        return true;
    }

};
#endif // STM32F4xx

#endif // SMARTDRIVE_CDNCTRANSPORT_H
