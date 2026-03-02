//
// Created by Immaculata on 02/03/2026.
//
// CDnCTransport.h
// Component Discovery and Communication (CDnC) Protocol Transport
//
// Abstract base for all CDnC physical implementations.
// Sits below AbstractTransport in the stack — concrete subclasses only need
// to implement exchangeByte() and CS control. All framing is handled above.
//
// Half-duplex, synchronous, single DATA line + CLK.
// Master always drives the clock. DATA line direction switches between TX and RX phases.
//
// One CDnC byte-exchange transaction:
//   [Assert CS] → [8 clks TX: master shifts out byte] → [turnaround] → [8 clks RX: master clocks in byte] → [Deassert CS]
//
// NOP byte = 0x00. Sister board sends 0x00 during master TX phase.
// Master sends 0x00 during RX-only phase (just clocking in data).
//

#ifndef SMARTDRIVE_CDNCTRANSPORT_H
#define SMARTDRIVE_CDNCTRANSPORT_H

#include "smartdrive/transport/AbstractTransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"

class CDnCTransport : public AbstractTransport
{

    uint8_t rxBuffer[ProtocolConstants::MAX_FRAME_SIZE];
    uint8_t rxHead = 0; // next byte to read (drain pointer)
    uint8_t rxTail = 0; // next free slot  (fill pointer)
    uint8_t rxCount = 0;

public:
    bool send(const SerializedData &data) override
    {
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

        for (size_t i = 0; i < data.size; ++i)
        {
            const uint8_t incoming = exchangeByte(data.data[i]);
            bufferRxByte(incoming);
        }

        deassertCS();
        return true;
    }

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
        const uint8_t byte = rxBuffer[rxHead];
        rxHead = (rxHead + 1) % ProtocolConstants::MAX_FRAME_SIZE;
        rxCount--;
        return byte;
    }

    // CDnC Primitives — subclasses implement these for their platform.
    //
    // exchangeByte(): drives 8 TX clocks then 8 RX clocks on the DATA line.
    //   out  — byte to transmit (0x00 = NOP if just clocking in)
    //   returns the byte received from the sister board
    //
    // assertCS() / deassertCS(): pull the chip-select line for the target
    //   component low/high. Subclass manages which CS pin is active.
    virtual uint8_t exchangeByte(uint8_t out) = 0;
    virtual void assertCS() = 0;
    virtual void deassertCS() = 0;

private:
    void bufferRxByte(const uint8_t byte)
    {
        if (rxCount >= ProtocolConstants::MAX_FRAME_SIZE)
        {
            // Buffer full — drop oldest byte (overrun)
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
