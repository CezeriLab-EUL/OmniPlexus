#ifndef SMARTDRIVE_ARDUINOCDNCTRANSPORT_H
#define SMARTDRIVE_ARDUINOCDNCTRANSPORT_H

#ifdef ARDUINO
#include <Arduino.h>
#include "CDnCTransport.h"

class ArduinoCDnCTransport : public CDnCTransport
{
    const uint8_t pinData;
    const uint8_t pinClk;
    const uint8_t pinCS;

    static constexpr uint32_t CLK_TIMEOUT_US = 100000UL;

public:
    ArduinoCDnCTransport(uint8_t dataPin, uint8_t clkPin, uint8_t csPin)
        : pinData(dataPin), pinClk(clkPin), pinCS(csPin) {}

    void begin()
    {
        pinMode(pinClk, INPUT);
        pinMode(pinCS, INPUT);
        pinMode(pinData, INPUT);
    }

    //  Synchronization Helpers
    bool waitForCS(uint32_t timeoutMs = 15000UL)
    {
        const uint32_t deadline = millis() + timeoutMs;
        while (digitalRead(pinCS) == HIGH)
        {
            if (millis() > deadline)
                return false;
        }
        return true;
    }

    bool isCSAsserted() const { return digitalRead(pinCS) == LOW; }

    void waitForCSRelease()
    {
        while (digitalRead(pinCS) == LOW)
            ;
    }

    //  AbstractTransport Implementation
    bool send(const SerializedData &data) override
    {
        if (data.size == 0)
            return false;
        for (size_t i = 0; i < data.size; ++i)
        {
            txByte(data.data[i]);
        }
        return true;
    }

protected:
    // This stops reading once the protocol's state machine (AbstractTransport)
    // detects that a full packet (Header + Length + Payload + CRC) has arrived.
    uint16_t bytesAvailable() override
    {
        return (isCSAsserted() && !hasCompleteFrame()) ? 1 : 0;
    }

    uint8_t readByte() override
    {
        return rxByte();
    }

    //  CDnC Bit-Bang Primitives
    void txByte(uint8_t out) override
    {
        pinMode(pinData, OUTPUT);
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            digitalWrite(pinData, (out >> bit) & 0x01 ? HIGH : LOW);
            waitForClkEdge(HIGH);
            waitForClkEdge(LOW);
        }
        pinMode(pinData, INPUT);
    }

    uint8_t rxByte() override
    {
        pinMode(pinData, INPUT);
        uint8_t result = 0;
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            if (!waitForClkEdge(HIGH))
                return 0;
            if (digitalRead(pinData))
                result |= (1 << bit);
            if (!waitForClkEdge(LOW))
                return 0;
        }
        return result;
    }

    void turnaround() override { pinMode(pinData, INPUT); }
    void assertCS() override {}
    void deassertCS() override {}

private:
    bool waitForClkEdge(uint8_t level)
    {
        const uint32_t deadline = micros() + CLK_TIMEOUT_US;
        while (digitalRead(pinClk) != level)
        {
            if (micros() > deadline)
                return false;
        }
        return true;
    }
};

#endif // ARDUINO
#endif // SMARTDRIVE_ARDUINOCDNCTRANSPORT_H