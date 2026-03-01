//
// Created by Immaculata on 02/03/2026.
//
// STM32CDnCTransport.h
// CDnC transport implementation for STM32F407 (master side).
//
// Uses STM32 HAL SPI in half-duplex master mode.
// DATA line direction is managed by switching between SPI TX and RX calls.
// CLK is always driven by this side (master).
//
// One CS pin per sister board is passed in at construction.
// To talk to a different component, construct a separate STM32CDnCTransport
// instance with a different CS pin, or add a setCSPin() method later.
//
// HAL SPI handle must be configured as:
//   Mode:            SPI_MODE_MASTER
//   Direction:       SPI_DIRECTION_1LINE (half-duplex)
//   Data size:       SPI_DATASIZE_8BIT
//   CLK polarity:    CPOL = LOW  (idle low)
//   CLK phase:       CPHA = 1st edge (sample on rising)
//   NSS:             Software (we manage CS manually)
//   Baud rate:       Start slow — e.g. APB2/256 (~328 kHz) for initial testing
//

#ifndef SMARTDRIVE_STM32CDNCTRANSPORT_H
#define SMARTDRIVE_STM32CDNCTRANSPORT_H

#if defined(STM32F4xx) || defined(USE_HAL_DRIVER)
#include "stm32f4xx_hal.h"
#include "CDnCTransport.h"

class STM32CDnCTransport : public CDnCTransport
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *csPort;
    uint16_t csPin;

    static constexpr uint32_t SPI_TIMEOUT_MS = 10;

public:
    // hspi    — pointer to your HAL SPI handle (e.g. &hspi1)
    // csPort  — GPIO port for chip select (e.g. GPIOA)
    // csPin   — GPIO pin for chip select  (e.g. GPIO_PIN_4)
    STM32CDnCTransport(SPI_HandleTypeDef *hspi, GPIO_TypeDef *csPort, uint16_t csPin)
        : hspi(hspi), csPort(csPort), csPin(csPin) {}

protected:
    // exchangeByte: half-duplex exchange using HAL SPI 1-line mode.
    // TX phase: HAL_SPI_Transmit (SPI drives MOSI, DATA line = output)
    // RX phase: HAL_SPI_Receive  (SPI reads MISO,  DATA line = input)
    //
    // The turnaround between TX and RX is handled automatically by the
    // 1-line HAL driver switching the data direction register (BIDIOE bit).
    uint8_t exchangeByte(uint8_t out) override
    {
        uint8_t received = ProtocolConstants::NOP_BYTE;

        // TX phase
        if (HAL_SPI_Transmit(hspi, &out, 1, SPI_TIMEOUT_MS) != HAL_OK)
        {
            LOG(LogLevel::ERROR, "CDnC STM32: SPI transmit failed");
            return ProtocolConstants::NOP_BYTE;
        }

        // RX phase — HAL switches BIDIOE, clocks in 1 byte
        if (HAL_SPI_Receive(hspi, &received, 1, SPI_TIMEOUT_MS) != HAL_OK)
        {
            LOG(LogLevel::ERROR, "CDnC STM32: SPI receive failed");
            return ProtocolConstants::NOP_BYTE;
        }

        return received;
    }

    void assertCS() override
    {
        HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_RESET); // Active low
    }

    void deassertCS() override
    {
        HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);
    }
};

#endif // STM32F4xx
#endif // SMARTDRIVE_STM32CDNCTRANSPORT_H
