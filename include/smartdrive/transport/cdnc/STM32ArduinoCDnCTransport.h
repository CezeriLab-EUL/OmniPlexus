//
// Created by Immaculata on 02/03/2026.
//
//
// STM32ArduinoCDnCTransport.h
// CDnC transport for STM32 using Arduino framework — master side.
//
// Bit-bangs the CDnC protocol using Arduino pinMode/digitalWrite.
// Compatible with any STM32 board running the Arduino STM32 core
// (PlatformIO framework = arduino, platform = ststm32).
//
// For bare-metal STM32 HAL projects, use STM32CDnCTransport.h instead.
//
// Master always drives CLK. DATA direction switches between TX and RX phases.
// One CS pin per sister board — pass different CS pins for different components.
//
// Pin wiring (STM32F407VET Black → Arduino Uno custom board):
//   PB3 (CLK)  → PD3
//   PB5 (DATA) → PD4
//   PB4 (CS)   → PD7
//   GND        → GND
//

#ifndef SMARTDRIVE_STM32ARDUINOCDNCTRANSPORT_H
#define SMARTDRIVE_STM32ARDUINOCDNCTRANSPORT_H

// #if defined(ARDUINO) && (defined(STM32F4xx) || defined(STM32F1xx) || defined(ARDUINO_ARCH_STM32))

#ifdef ARDUINO

#include <Arduino.h>
#include "CDnCTransport.h"

class STM32ArduinoCDnCTransport : public CDnCTransport
{
    const uint32_t pinData;
    const uint32_t pinClk;
    const uint32_t pinCS;

    // Half-period of CLK in microseconds.
    // 200us = ~2.5kHz. Tighten to 50 or 5 once transport is verified.
    uint32_t clkHalfPeriodUs;

    // How long to hold the turnaround before starting RX clocks.
    // Must be long enough for slave to finish Serial.flush() and
    // switch DATA pin to OUTPUT. 100ms is generous — tighten later.
    uint32_t turnaroundMs;

public:
    STM32ArduinoCDnCTransport(uint32_t dataPin,
                              uint32_t clkPin,
                              uint32_t csPin,
                              uint32_t clkHalfPeriodUs = 14,
                              uint32_t turnaroundMs = 7)
        : pinData(dataPin),
          pinClk(clkPin),
          pinCS(csPin),
          clkHalfPeriodUs(clkHalfPeriodUs),
          turnaroundMs(turnaroundMs)
    {
    }

    void begin()
    {
        pinMode(pinClk, OUTPUT);
        pinMode(pinCS, OUTPUT);
        pinMode(pinData, OUTPUT);

        digitalWrite(pinClk, LOW); // CLK idle low
        digitalWrite(pinCS, HIGH); // CS deasserted
        digitalWrite(pinData, LOW);
    }

    // Adjust clock speed at runtime once transport is proven working
    void setClkHalfPeriod(uint32_t us) { clkHalfPeriodUs = us; }
    void setTurnaroundMs(uint32_t ms) { turnaroundMs = ms; }

protected:
    // txByte() — master clocks out one byte MSB first.
    // Data is set before rising edge so slave can sample on rise.
    void txByte(uint8_t out) override
    {
        pinMode(pinData, OUTPUT);
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            digitalWrite(pinData, (out >> bit) & 0x01 ? HIGH : LOW);
            delayMicroseconds(clkHalfPeriodUs); // data setup time
            digitalWrite(pinClk, HIGH);
            delayMicroseconds(clkHalfPeriodUs); // hold high
            digitalWrite(pinClk, LOW);
            delayMicroseconds(clkHalfPeriodUs); // hold low before next bit
        }
    }

    // rxByte() — master clocks in one byte MSB first.
    // DATA switches to INPUT. Master generates clock, samples on rising edge.
    uint8_t rxByte() override
    {
        pinMode(pinData, INPUT);
        uint8_t result = 0;
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            delayMicroseconds(clkHalfPeriodUs);
            digitalWrite(pinClk, HIGH);
            delayMicroseconds(clkHalfPeriodUs);
            if (digitalRead(pinData))
                result |= (1 << bit);
            digitalWrite(pinClk, LOW);
            delayMicroseconds(clkHalfPeriodUs);
        }
        return result;
    }

    // turnaround() — master releases DATA and waits.
    // This gap gives the slave time to:
    //   1. Finish processing the received frame
    //   2. Build its response
    //   3. Call Serial.flush() if it prints anything
    //   4. Switch DATA pin to OUTPUT and pre-set the first bit
    void turnaround() override
    {
        pinMode(pinData, INPUT);   // release DATA line
        digitalWrite(pinClk, LOW); // keep CLK low during turnaround
        delay(turnaroundMs);
    }

    void assertCS() override
    {
        digitalWrite(pinCS, LOW);
        delayMicroseconds(clkHalfPeriodUs); // CS setup time before first clock
    }

    void deassertCS() override
    {
        digitalWrite(pinCS, HIGH);
    }
};

#endif // ARDUINO && STM32
#endif // SMARTDRIVE_STM32ARDUINOCDNCTRANSPORT_H
