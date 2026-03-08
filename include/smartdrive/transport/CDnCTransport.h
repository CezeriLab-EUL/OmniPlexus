//
// Created by Immaculata on 02/03/2026.
//
// CDnCTransport.h
// Component Discovery and Communication (CDnC) Protocol Transport
//
// Abstract base for all CDnC physical implementations.
// Sits below AbstractTransport in the stack — concrete subclasses only need
// to implement txByte(), rxByte(), turnaround(), assertCS(), deassertCS().
//
// Half-duplex, synchronous, single DATA line + CLK.
// Master always drives CLK. DATA direction switches between TX and RX phases.
//
// Full transaction for one frame:
//   [Assert CS]
//   [TX phase]   master clocks out every byte of the serialized frame
//   [Turnaround] master releases DATA, slave switches to OUTPUT
//   [RX phase]   master clocks in slave response byte by byte
//   [Deassert CS]
//
// TX and RX phases are completely separate — this is the fundamental difference
// from full-duplex SPI where exchangeByte() does both simultaneously.
//

#ifndef SMARTDRIVE_CDNCTRANSPORT_H
#define SMARTDRIVE_CDNCTRANSPORT_H

#include "smartdrive/transport/AbstractTransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"

class CDnCTransport : public AbstractTransport
{
    // RX ring buffer — filled during send() from the slave's response.
    // AbstractTransport::accumulate() drains this via bytesAvailable()/readByte().
    uint8_t rxBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t rxHead = 0;
    uint8_t rxTail = 0;
    uint8_t rxCount = 0;

public:
    // send() — transmits the frame then immediately clocks in the slave response.
    // Both TX and RX happen within a single CS assertion.
    // The slave response is buffered in rxBuffer for accumulate() to process.
    bool send(const SerializedData &data) override
    {
        clearRxBuffer();

        if (data.size == 0)
        {
            LOG(LogLevel::ERROR, "CDnC: send() called with empty frame");
            return false;
        }

        if (data.size > ProtocolConstants::MAX_FRAME_SIZE)
        {
            LOG(LogLevel::ERROR, "CDnC: frame exceeds MAX_FRAME_SIZE");
            return false;
        }

        assertCS();

        // TX phase — clock out every byte of the serialized frame
        for (size_t i = 0; i < data.size; ++i)
        {
            txByte(data.data[i]);
        }

        // Turnaround — release DATA, give slave time to switch pin to OUTPUT
        turnaround();

        // RX phase — clock in slave response into rxBuffer.
        // Frame is self-describing: byte[0] = header, byte[1] = payload length.
        // Total frame size = 2 + payloadLength + 1 (CRC).
        // Read first 2 bytes to learn exact size, then read the rest.
        const uint8_t header = rxByte();
        const uint8_t lengthByte = rxByte();

        bufferRxByte(header);
        bufferRxByte(lengthByte);

        if (ProtocolConstants::isValidHeader(header) &&
            lengthByte > 0 &&
            lengthByte <= ProtocolConstants::MAX_PAYLOAD_SIZE)
        {
            // payload bytes + 1 CRC byte
            const uint8_t remaining = lengthByte + ProtocolConstants::CRC_OFFSET;
            for (uint8_t i = 0; i < remaining; ++i)
            {
                bufferRxByte(rxByte());
            }
        }
        // If header is invalid (slave sent NOP / not ready), the accumulator
        // state machine discards the bytes cleanly — no special handling needed.

        deassertCS();
        return true;
    }

    // CDnC primitives — subclasses implement these per platform.
    //
    // txByte(out)  — clock out one byte MSB first. DATA line = OUTPUT.
    // rxByte()     — clock in one byte MSB first. DATA line = INPUT. Returns byte.
    // turnaround() — release DATA line. Wait long enough for slave to react
    //                and switch its DATA pin to OUTPUT before first RX clock.
    // assertCS()   — pull CS low  (active low, selects target component)
    // deassertCS() — pull CS high (deselects target component)
    virtual void txByte(uint8_t out) = 0;
    virtual uint8_t rxByte() = 0;
    virtual void turnaround() = 0;
    virtual void assertCS() = 0;
    virtual void deassertCS() = 0;

protected:
    // AbstractTransport hooks — these feed the framing state machine in
    // AbstractTransport::accumulate(). Drains rxBuffer rather than reading
    // live hardware, because bytes were already captured during send() above.
    uint16_t bytesAvailable() override
    {
        return rxCount;
    }

    uint8_t readByte() override
    {
        if (rxCount == 0)
        {
            LOG(LogLevel::WARNING, "CDnC: readByte() called with empty rx buffer");
            return ProtocolConstants::NOP_BYTE;
        }
        const uint8_t b = rxBuffer[rxHead];
        rxHead = (rxHead + 1) % ProtocolConstants::MAX_FRAME_SIZE;
        rxCount--;
        return b;
    }

    void clearRxBuffer()
    {
        rxHead = 0;
        rxTail = 0;
        rxCount = 0;
    }

private:
    void bufferRxByte(const uint8_t byte)
    {
        if (rxCount >= ProtocolConstants::MAX_FRAME_SIZE)
        {
            LOG(LogLevel::WARNING, "CDnC: RX buffer overrun, dropping oldest byte");
            rxHead = (rxHead + 1) % ProtocolConstants::MAX_FRAME_SIZE;
            rxCount--;
        }
        rxBuffer[rxTail] = byte;
        rxTail = (rxTail + 1) % ProtocolConstants::MAX_FRAME_SIZE;
        rxCount++;
    }
};

#endif // SMARTDRIVE_CDNCTRANSPORT_H
