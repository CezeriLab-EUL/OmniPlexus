#ifndef SMARTDRIVE_PARALLELCDNCTRANSPORT_H
#define SMARTDRIVE_PARALLELCDNCTRANSPORT_H

#include "smartdrive/transport/AbstractTransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"

class ParallelCDnCTransport
{
public:
    static constexpr uint8_t MAX_SLAVES = 16;
    static constexpr uint8_t BUF_SIZE = ProtocolConstants::MAX_FRAME_SIZE;

    struct SlaveAccumulator
    {
        enum class State : uint8_t
        {
            WAITING_FOR_HEADER,
            WAITING_FOR_LENGTH,
            READING_PAYLOAD,
            VALIDATING_CRC,
            FRAME_READY
        }
    };

    uint8_t buf[BUF_SIZE] = {};
    uint8_t ready[BUF_SIZE] = {};
    uint8_t collected = 0;
    uint8_t payloadLen = 0;
    bool frameConsumed = true;
    SlaveAccumulator::State state = SlaveAccumulator::State::WAITING_FOR_HEADER;

    void reset()
    {
        collected = 0;
        payloadLen = 0;
        state = SlaveAccumulator::State::WAITING_FOR_HEADER;
    }

    void processByte(uint8_t byte)
    {
        switch (state)
        {
        case SlaveAccumulator::State::WAITING_FOR_HEADER:
            if (ProtocolConstants::isValidHeader(byte))
            {
                collected = 0;
                buf[collected++] = byte;
                state = SlaveAccumulator::State::WAITING_FOR_LENGTH;
            }
            // NOP bytes silently discarded
            break;

        case SlaveAccumulator::State::WAITING_FOR_LENGTH:
            if (byte == 0 || byte > ProtocolConstants::MAX_PAYLOAD_SIZE)
            {
                LOG(LogLevel::ERROR, "ParallelCDnC: Invalid length, resseting accumulator");
                reset();
                break;
            }
            payloadLen = byte;
            buf[collected++] = byte;
            state = SlaveAccumulator::State::READING_PAYLOAD;
            break;

        case SlaveAccumulator::State::READING_PAYLOAD:
            buf[collected++] = byte;
            if (collected == payloadLen + 2)
            { // +2 for header and length bytes
                state = SlaveAccumulator::State::VALIDATING_CRC;
            }
            break;

        case SlaveAccumulator::State::VALIDATING_CRC:
        {
            buf[collected++] = byte;
            RawData toCheck{buf, static_cast<size_t>(collected - 1)}; // Exclude CRC byte
            const uint8_t calc = CRC8::compute(toCheck);
            if (calc != byte)
            {
                LOG(LogLevel::ERROR, "ParallelCDnC: CRC mismatch, resseting accumulator");
                reset();
            }
            else if (!frameConsumed)
            {
                LOG(LogLevel::WARNING, "ParallelCDnC: Frame not consumed, dropping");
                reset();
            }
            else
            {
                memccpy(ready, buf, collected);
                frameConsumed = false;
                state = SlaveAccumulator::State::FRAME_READY;
            }
            break;
        }

        case SlaveAccumulator::State::FRAME_READY:
            break; // stall until caller calls releaseFrame()
        }
    }

    bool hasFrame() const { return state == SlaveAccumulator::State::FRAME_READY; }

    RawData getFrame() { return RawData{ready, collected}; }
}