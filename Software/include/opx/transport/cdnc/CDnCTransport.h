// CDnCTransport.h
// ITransport implementation wrapping the CDnC half-duplex parallel bus.
// One instance per slave (index 0–15). Slave index == transport ID.
//
// NOTE: cdnc_exchange() must be called from the main firmware loop separately.
// CDnCTransport::accumulate() only drains the software RX ring buffers;
// it does not drive the physical bus.

#ifndef SMARTDRIVE_CDNCTRANSPORT_H
#define SMARTDRIVE_CDNCTRANSPORT_H

#if defined(STM32F4xx) || defined(STM32F1xx)

#include "opx/interfaces/ITransport.h"
#include "opx/constants/ProtocolConstants.h"
#include "opx/utils/Logger.h"
#include "CDnC.h"

class CDnCTransport : public ITransport {
public:
    explicit CDnCTransport(uint8_t slaveIndex)
    : _slaveIndex(slaveIndex)
    {
        resetParser();
    }

    // ITransport — TX
    bool send(const SerializedData& data) override {
        if (_slaveIndex == CDNC_SLAVE_BROADCAST) {
            return sendBroadcast(data);
        }

        uint32_t slotsNeeded = (uint32_t)data.size * 8;
        uint32_t w = cdnc_write_ptr(_slaveIndex);
        uint32_t r = cdnc_read_ptr();
        if ((w - r) + slotsNeeded > CDNC_TX_BUF_SIZE) {
            LOG(LogLevel::OP_ERROR, "CDnCTransport: TX buffer would overflow");
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

    RawData getFrame() override {
        return RawData{ _frameBuf, _framePos };
    }

    void releaseFrame() override {
        _frameReady = false;
        resetParser();
    }

    uint8_t slaveIndex()  const { return _slaveIndex; }
    bool    isBroadcast() const { return _slaveIndex == CDNC_SLAVE_BROADCAST; }

private:
    // OPX CRC-scanning frame parser
    // Reconstructs OPX frames from the raw byte stream.
    // Discards 0x00 and 0xFF bytes while waiting for a valid header.
    // Once a valid header is accepted, accumulates until CRC matches.

    uint8_t    _slaveIndex;
    uint8_t    _frameBuf[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t    _framePos  = 0;
    bool       _frameReady = false;

    // Minimum frame sizes by type (header + payload + CRC)
    static constexpr uint8_t MIN_COMMAND_FRAME  = 5;  // hdr+seqNum+cmdLo+cmdHi+CRC
    static constexpr uint8_t MIN_RESPONSE_FRAME = 6;
    static constexpr uint8_t MIN_OTHER_FRAME    = 5;

    uint8_t minFrameSize() const {
        if (_framePos == 0) return MIN_COMMAND_FRAME;
        const ProtocolConstants::FrameType ft =
        ProtocolConstants::decodeType(_frameBuf[0]);
        switch (ft) {
            case ProtocolConstants::FrameType::COMMAND:  return MIN_COMMAND_FRAME;
            case ProtocolConstants::FrameType::RESPONSE: return MIN_RESPONSE_FRAME;
            default:                                      return MIN_OTHER_FRAME;
        }
    }

    // Feed one byte through the CRC-scanning parser.
    // Returns true when a complete valid frame is in _frameBuf.
    bool parseByte(uint8_t byte) {
        // While waiting for a header, discard NOP bytes and invalid headers
        if (_framePos == 0) {
            if (byte == ProtocolConstants::NOP_BYTE) return false;
            if (byte == 0x00)                        return false;
            if (!ProtocolConstants::isValidHeader(byte) ||
                !ProtocolConstants::isValidFrameType(byte)) {
                return false;
                }
        }

        // Overflow guard
        if (_framePos >= ProtocolConstants::MAX_FRAME_SIZE) {
            resetParser();
            return false;
        }

        _frameBuf[_framePos++] = byte;

        // Try CRC match once we have enough bytes for this frame type
        if (_framePos >= minFrameSize()) {
            // CRC is over all bytes except the last (which is the CRC itself)
            RawData candidate{ _frameBuf, static_cast<uint16_t>(_framePos - 1) };
            uint8_t computed = CRC8::compute(candidate);
            if (computed == _frameBuf[_framePos - 1]) {
                _frameReady = true;
                return true;
            }
        }

        return false;
    }

    void resetParser() {
        _framePos  = 0;
    }

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

#endif // STM32F4xx || STM32F1xx

#endif // SMARTDRIVE_CDNCTRANSPORT_H
