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

    // worst case scenario timeout
    static constexpr uint32_t READY_TIMEOUT_US = 200000UL;

    struct SlaveAccumulator
    {
        enum class State : uint8_t
        {
            WAITING_FOR_HEADER,
            WAITING_FOR_LENGTH,
            READING_PAYLOAD,
            VALIDATING_CRC,
            FRAME_READY
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
                    LOG(LogLevel::OP_ERROR, "ParallelCDnC: Invalid length, resseting accumulator");
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
                    LOG(LogLevel::OP_ERROR, "ParallelCDnC: CRC mismatch, resseting accumulator");
                    reset();
                }
                else if (!frameConsumed)
                {
                    LOG(LogLevel::OP_WARNING, "ParallelCDnC: Frame not consumed, dropping");
                    reset();
                }
                else
                {
                    memccpy(ready, buf, 0, collected); // CHECK
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

        void releaseFrame()
        {
            frameConsumed = true;
            reset();
        }
    };

    // queue serialized frame for delivery to one slave next cycle
    bool queueFrame(uint8_t slaveIdx, const SerializedData &data)
    {
        if (slaveIdx >= ParallelCDnCTransport::MAX_SLAVES)
        {
            LOG(LogLevel::OP_ERROR, "ParallelCDnC: slaveIdx out of range");
            return false;
        }
        if (data.size == 0 || data.size > ParallelCDnCTransport::BUF_SIZE)
        {
            LOG(LogLevel::OP_ERROR, "ParallelCDnC: invalid frame size");
            return false;
        }
        if (txCount[slaveIdx] + data.size > ParallelCDnCTransport::BUF_SIZE)
        {
            LOG(LogLevel::OP_WARNING, "ParallelCDnC: TX buffer full for slave");
            return false;
        }

        for (size_t i = 0; i < data.size; ++i)
        {
            slaveTxBuffer[slaveIdx][txWritePtr[slaveIdx]] = data.data[i];
            txWritePtr[slaveIdx] = (txWritePtr[slaveIdx] + 1) % ParallelCDnCTransport::BUF_SIZE;
        }
        txCount[slaveIdx] += static_cast<uint8_t>(data.size);
        return true;
    }

    // Run one full TX→turnaround→RX cycle across all 16 slaves.
    void cycle()
    {
        const uint8_t cycleBytes = computeCycleLength();
        runCycle(cycleBytes == 0 ? 1 : cycleBytes);
    }

    // per-slave frame access — called after cycle() returns.
    bool hasFrame(uint8_t slaveIdx) const { return accumulators[slaveIdx].hasFrame(); }
    RawData getFrame(uint8_t slaveIdx) { return accumulators[slaveIdx].getFrame(); }
    void releaseFrame(uint8_t slaveIdx) { accumulators[slaveIdx].releaseFrame(); }

    // Presence map — set during discovery, bit N = slave N present.
    void markPresent(uint8_t slaveIdx) { present |= (1u << slaveIdx); }
    void markAbsent(uint8_t slaveIdx) { present &= ~(1u << slaveIdx); }
    bool isPresent(uint8_t slaveIdx) const { return (present >> slaveIdx) & 0x01; }
    uint16_t presenceMap() const { return present; }

protected:
    virtual void setDataPortOutput() = 0;
    virtual void setDataPortInput() = 0;
    virtual void writeDataPort(uint16_t v) = 0;
    virtual uint16_t readDataPort() = 0;
    virtual void setClk(bool high) = 0;
    virtual void waitHalfPeriod() = 0; // one CLK half-period delay
    virtual void waitTurnaround() = 0; // slave pin-switch settling time
    virtual uint32_t currentUs() = 0;  // Monotonic microsecond counter used only for the ready-wait watchdog.

private:
    // tx buffers
    uint8_t slaveTxBuffer[MAX_SLAVES][BUF_SIZE] = {};
    uint8_t txWritePtr[MAX_SLAVES] = {};
    uint8_t txReadPtr[MAX_SLAVES] = {};
    uint8_t txCount[MAX_SLAVES] = {};

    // rx buffers
    uint16_t recvBits[BUF_SIZE * 8] = {};
    SlaveAccumulator accumulators[MAX_SLAVES] = {};

    // presence map
    uint16_t present = 0x0000; // all absent until discovery

    // internal helpers
    uint8_t computeCycleLength() const
    {
        uint8_t maxLen = 0;
        for (uint8_t s = 0; s < MAX_SLAVES; ++s)
        {
            if (txCount[s] > maxLen)
            {
                maxLen = txCount[s];
            }
        }
        return maxLen;
    }

    void runCycle(uint8_t cycleBytes)
    {
        const uint8_t totalBits = cycleBytes * 8;

        // transpose
        uint16_t sendbuffer[BUF_SIZE * 8] = {};

        for (uint8_t s = 0; s < MAX_SLAVES; ++s)
        {
            for (uint8_t byteIdx = 0; byteIdx < cycleBytes; ++byteIdx)
            {
                uint8_t txByte = 0x00; // default: NOP / padding

                if (txCount[s] > 0)
                {
                    txByte = slaveTxBuffer[s][txReadPtr[s]];
                    txReadPtr[s] = (txReadPtr[s] + 1) % BUF_SIZE;
                    txCount[s]--;
                }

                // Pack each bit of txByte into the corresponding sendbuffer slot.
                // MSB first: bit 7 goes into sendbuffer[byteIdx*8 + 0], etc.
                for (uint8_t bit = 0; bit < 8; ++bit)
                {
                    const uint8_t bitPos = byteIdx * 8 + bit;
                    const uint8_t bitVal = (txByte >> (7 - bit)) & 0x01;
                    sendbuffer[bitPos] |= (static_cast<uint16_t>(bitVal) << s);
                }
            }
        }

        // tx phase
        setDataPortOutput();

        for (uint8_t i = 0; i < totalBits; ++i)
        {
            writeDataPort(sendbuffer[i]);
            waitHalfPeriod(); // data setup time
            setClk(true);
            waitHalfPeriod(); // hold high — slaves sample here
            setClk(false);
            waitHalfPeriod(); // hold low before next bit
        }

        // turnaround
        setDataPortInput();
        waitTurnaround();

        // ready wait
        //  Poll DATA port for any present slave driving its pin HIGH.
        bool rxValid = false;

        if (present != 0x0000)
        {
            const uint32_t readyStart = currentUs();

            while ((currentUs() - readyStart) < READY_TIMEOUT_US)
            {
                // Any present slave driving DATA HIGH = ready
                if ((readDataPort() & present) != 0)
                {
                    rxValid = true;
                    break;
                }
            }

            if (!rxValid)
            {
                LOG(LogLevel::OP_WARNING, "ParallelCDnC: ready timeout — skipping RX");
            }
        }

        if (!rxValid)
            return;

        // rx phase
        //  Slave has pre-set its first bit. Start clocking immediately.
        //  Sample DATA on every rising CLK edge.

        for (uint8_t i = 0; i < totalBits; ++i)
        {
            waitHalfPeriod();
            setClk(true);
            waitHalfPeriod();
            recvBits[i] = readDataPort(); // sample on rising edge
            setClk(false);
            waitHalfPeriod();
        }

        // upack + accumulate
        //  Only reconstruct and accumulate bytes for present slaves.
        for (uint8_t s = 0; s < MAX_SLAVES; ++s)
        {
            if (!isPresent(s))
                continue;

            for (uint8_t byteIdx = 0; byteIdx < cycleBytes; ++byteIdx)
            {
                uint8_t rxByte = 0;
                for (uint8_t bit = 0; bit < 8; ++bit)
                {
                    const uint8_t bitPos = byteIdx * 8 + bit;
                    const uint8_t bitVal = (recvBits[bitPos] >> s) & 0x01;
                    rxByte |= (bitVal << (7 - bit));
                }
                accumulators[s].processByte(rxByte);
            }
        }
    }
};

#endif // SMARTDRIVE_PARALLELCDNCTRANSPORT_H