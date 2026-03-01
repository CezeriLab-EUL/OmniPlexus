//
// Created by Immaculata on 02/03/2026.
//
// ArduinoCDnCTransport.h
// CDnC transport implementation for Arduino (AVR) platforms.
//
// Used on the sister board side (e.g. Arduino Uno acting as indicator board
// during STM32 ↔ Uno testing).
//
// Bit-bangs the CDnC protocol: DATA pin switches direction between TX and RX.
// CLK is always driven by the master (STM32). This side only drives DATA
// during its TX phase and reads DATA during the master's TX phase.
//
// Pin wiring (example, configure in constructor):
//   DATA  → any digital pin (configured as INPUT or OUTPUT dynamically)
//   CLK   → any digital pin (INPUT — master drives clock)
//   CS    → any digital pin (INPUT — master asserts)
//

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

    // MSB first, data sampled on rising edge, shifted out on falling edge
    static constexpr bool MSB_FIRST = true;
    static constexpr uint8_t TURNAROUND_CYCLES = 2; // CLK low cycles between TX and RX

public:
    ArduinoCDnCTransport(uint8_t dataPin, uint8_t clkPin, uint8_t csPin)
        : pinData(dataPin), pinClk(clkPin), pinCS(csPin) {}

    void begin()
    {
        // CS and CLK are inputs on the sister board — master drives them
        pinMode(pinClk, INPUT);
        pinMode(pinCS, INPUT);
        // DATA starts as input, direction managed per-transaction
        pinMode(pinData, INPUT);
    }

protected:
    // exchangeByte: transmit `out`, receive and return 1 byte.
    // Half-duplex: TX phase first (DATA = OUTPUT), then RX phase (DATA = INPUT).
    // Master drives CLK throughout. Sister board just follows.
    uint8_t exchangeByte(uint8_t out) override
    {
        uint8_t received = 0;

        // TX phase: shift out `out`, MSB first
        pinMode(pinData, OUTPUT);
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            // Set data bit before rising edge
            digitalWrite(pinData, (out >> bit) & 0x01 ? HIGH : LOW);
            // Wait for master's rising edge (sample point)
            waitForClkEdge(HIGH);
            // Wait for falling edge before next bit
            waitForClkEdge(LOW);
        }

        // Turnaround: release DATA line, wait for master to switch
        pinMode(pinData, INPUT);
        for (uint8_t i = 0; i < TURNAROUND_CYCLES; ++i)
        {
            waitForClkEdge(HIGH);
            waitForClkEdge(LOW);
        }

        // RX phase: sample incoming bits on rising edge
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            waitForClkEdge(HIGH);
            if (digitalRead(pinData))
            {
                received |= (1 << bit);
            }
            waitForClkEdge(LOW);
        }

        return received;
    }

    // On the sister board, CS is asserted by the master — we don't drive it.
    // These are no-ops here; the master's CDnCTransport asserts/deasserts CS.
    void assertCS() override { /* master controls CS */ }
    void deassertCS() override { /* master controls CS */ }

private:
    // Spin-wait for CLK to reach `level`. Timeout after ~10000 iterations to
    // avoid locking up if master disappears mid-transaction.
    void waitForClkEdge(uint8_t level)
    {
        uint16_t timeout = 10000;
        while (digitalRead(pinClk) != level && --timeout)
        {
        }
        if (timeout == 0)
        {
            LOG(LogLevel::ERROR, "CDnC: CLK timeout waiting for edge");
        }
    }
};

#endif // ARDUINO
#endif // SMARTDRIVE_ARDUINOCDNCTRANSPORT_H
