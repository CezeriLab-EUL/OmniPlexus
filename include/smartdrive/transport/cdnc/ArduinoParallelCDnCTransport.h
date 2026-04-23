#ifndef SMARTDRIVE_ARDUINOPARALLELCDNCTRANSPORT_H
#define SMARTDRIVE_ARDUINOPARALLELCDNCTRANSPORT_H

#ifdef ARDUINO

#include <Arduino.h>
#include "smartdrive/transport/AbstractTransport.h"
#include "smartdrive/types/ProtocolTypes.h"
#include "smartdrive/utils/Logger.h"

class ArduinoParallelCDnCTransport : public AbstractTransport
{
public:
    // constructor
    ArduinoParallelCDnCTransport(
        uint8_t dataPin,
        uint8_t clkPin,
        uint32_t clkHalfPeriodUs = 50,
        uint32_t turnaroundUs = 25000,
        uint32_t clkIdleTimeoutUs = 100000UL)
        : pinData(dataPin),
          pinClk(clkPin),
          clkHalfPeriodUs(clkHalfPeriodUs),
          turnaroundUs(turnaroundUs),
          clkIdleTimeoutUs(clkIdleTimeoutUs)
    {
    }

    // initialisation
    void begin()
    {
        pinMode(pinClk, INPUT);  // CLK is always driven by master
        pinMode(pinData, INPUT); // DATA starts as INPUT (RX mode)
    }

    // runtime
    void setClkHalfPeriod(uint32_t us) { clkHalfPeriodUs = us; }
    void setTurnaround(uint32_t us) { turnaroundUs = us; }
    void setClkIdleTimeout(uint32_t us) { clkIdleTimeoutUs = us; }

    bool waitForFrame()
    {
        clearRxBuffer();
        uint8_t header = 0;
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            waitForClkLow();
            const uint32_t edgeTimeout = (bit == 7)
                                             ? clkIdleTimeoutUs
                                             : clkIdleTimeoutUs / 4;

            if (!waitForClkHigh(edgeTimeout))
            {
                // No CLK activity — bus is idle, let update() breathe
                return false;
            }

            if (digitalRead(pinData))
                header |= (1 << bit);
        }

        // validete header
        // A zero header (0x00) means master sent a NOP to this slot.
        // An invalid STX pattern means the bus was caught mid-frame or noise.
        // Either way: don't respond, wait for next frame.

        if (!ProtocolConstants::isValidHeader(header))
        {
            LOG(LogLevel::OP_WARNING, "ArduinoParallelCDnC: invalid header, ignoring");
            return false;
        }
        bufferRxByte(header);

        // recieve lenth byte
        uint8_t length = 0;
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            waitForClkLow();
            if (!waitForClkHigh(clkIdleTimeoutUs / 4))
            {
                LOG(LogLevel::OP_ERROR, "ArduinoParallelCDnC: CLK stall after header");
                return false;
            }
            if (digitalRead(pinData))
                length |= (1 << bit);
        }

        if (length == 0 || length > ProtocolConstants::MAX_PAYLOAD_SIZE)
        {
            LOG(LogLevel::OP_ERROR, "ArduinoParallelCDnC: invalid length byte");
            return false;
        }
        bufferRxByte(length);

        // recieve payload + CRC
        const uint8_t remaining = length + ProtocolConstants::CRC_SIZE;
        for (uint8_t byteNum = 0; byteNum < remaining; ++byteNum)
        {
            uint8_t b = 0;
            for (int8_t bit = 7; bit >= 0; --bit)
            {
                waitForClkLow();
                if (!waitForClkHigh(clkIdleTimeoutUs / 4))
                {
                    LOG(LogLevel::OP_ERROR, "ArduinoParallelCDnC: CLK stall mid-payload");
                    return false;
                }
                if (digitalRead(pinData))
                    b |= (1 << bit);
            }
            bufferRxByte(b);
        }

        return true;
    }

    // overidden send from abstract transport
    /* Called by CommunicationManager when _handleCommand fires _sendAck/_sendNack.
    At this point the master is in its turnaround gap (CLK LOW, GPIOE = INPUT).
    Slave switches DATA to OUTPUT, waits for master to resume clocking,
    and shifts out every byte of `data` MSB-first on rising CLK edges.
    */

    bool send(const SerializedData &data) override
    {
        if (data.size == 0 || data.size > ProtocolConstants::MAX_FRAME_SIZE)
        {
            LOG(LogLevel::OP_ERROR, "ArduinoParallelCDnC: send() bad size");
            return false;
        }

        waitForClkLow(); // should already be LOW, returns immediately if so
        delayMicroseconds(turnaroundUs / 2);

        pinMode(pinData, OUTPUT);
        const uint8_t firstBit = (data.data[0] >> 7) & 0x01;
        digitalWrite(pinData, firstBit ? HIGH : LOW);

        // clock out remaining bytes
        for (size_t byteIdx = 0; byteIdx < data.size; ++byteIdx)
        {
            const uint8_t b = data.data[byteIdx];
            for (int8_t bit = 7; bit >= 0; --bit)
            {
                // DATA is pre-set. Wait for master's rising edge.
                waitForClkHigh(0); // 0 = no timeout, master WILL clock

                waitForClkLow();

                // Pre-set next bit so it's stable before the next rising edge.
                uint8_t nextBit = 0;
                if (bit > 0)
                {
                    // Next bit is within the same byte
                    nextBit = (b >> (bit - 1)) & 0x01;
                }
                else if (byteIdx + 1 < data.size)
                {
                    // Next bit is bit 7 of the next byte
                    nextBit = (data.data[byteIdx + 1] >> 7) & 0x01;
                }
                // else: last bit of last byte — nextBit stays 0 (don't care)
                digitalWrite(pinData, nextBit ? HIGH : LOW);
            }
        }

        // release data line
        pinMode(pinData, 0x00); // INPUT
        return true;
    }

protected:
    uint16_t bytesAvailable() override
    {
        return rxCount;
    }

    uint8_t readByte() override
    {
        if (rxCount == 0)
        {
            LOG(LogLevel::OP_WARNING, "ArduinoParallelCDnC: readByte() on empty buffer");
            return ProtocolConstants::NOP_BYTE;
        }
        const uint8_t b = rxBuffer[rxHead];
        rxHead = (rxHead + 1) % ProtocolConstants::MAX_FRAME_SIZE;
        rxCount--;
        return b;
    }

private:
    // pin config
    const uint8_t pinData;
    const uint8_t pinClk;

    // timing
    uint32_t clkHalfPeriodUs;
    uint32_t turnaroundUs;
    uint32_t clkIdleTimeoutUs;

    // rx ring buffer
    uint8_t rxBuffer[ProtocolConstants::MAX_FRAME_SIZE] = {};
    uint8_t rxHead = 0;
    uint8_t rxTail = 0;
    uint8_t rxCount = 0;

    void clearRxBuffer()
    {
        rxHead = 0;
        rxTail = 0;
        rxCount = 0;
    }

    void bufferRxByte(uint8_t byte)
    {
        if (rxCount >= ProtocolConstants::MAX_FRAME_SIZE)
        {
            LOG(LogLevel::OP_WARNING, "ArduinoParallelCDnC: RX buffer overrun");
            rxHead = (rxHead + 1) % ProtocolConstants::MAX_FRAME_SIZE;
            rxCount--;
        }
        rxBuffer[rxTail] = byte;
        rxTail = (rxTail + 1) % ProtocolConstants::MAX_FRAME_SIZE;
        rxCount++;
    }

    // clk edge helpers

    // No timeout — master always brings CLK LOW after a HIGH.
    inline void waitForClkLow()
    {
        while (digitalRead(pinClk) == HIGH)
            ;
    }

    // Wait for CLK to go HIGH (rising edge).
    // timeoutUs = 0  →  wait forever (used in TX phase where master WILL clock).
    // timeoutUs > 0  →  return false if CLK stays LOW beyond this duration.

    // Returns true if CLK went HIGH, false on timeout.
    inline bool waitForClkHigh(uint32_t timeoutUs)
    {
        if (timeoutUs == 0)
        {
            while (digitalRead(pinClk) == LOW)
                ;
            return true;
        }
        const uint32_t start = micros();
        while (digitalRead(pinClk) == LOW)
        {
            if (micros() - start >= timeoutUs)
                return false;
        }
        return true;
    }
};

#endif // ARDUINO
#endif // SMARTDRIVE_ARDUINOPARALLELCDNCTRANSPORT_H