#ifndef SMARTDRIVE_STM32PARALLELCDNCTRANSPORT_H
#define SMARTDRIVE_STM32PARALLELCDNCTRANSPORT_H

#if defined(STM32F4xx) || defined(STM32F7407xx)

#include "stm32f4xx_hal.h"
#include "ParallelCDnCTransport.h"

class STM32ParallelCDnCTransport : public ParallelCDnCTransport
{
public:
    STM32ParallelCDnCTransport(
        GPIO_TypeDef *dataPort,
        uint16_t dataMask,
        GPIO_TypeDef *clkPort,
        uint16_t clkPin,
        uint32_t clkHalfPeriodUs = 50, // 2
        uint32_t turnaroundUs = 25000) // 500
        : dataPort_(dataPort),
          dataMask_(dataMask),
          clkPort_(clkPort),
          clkPin_(clkPin),
          clkHalfPeriodUs_(clkHalfPeriodUs),
          turnaroundUs_(turnaroundUs)
    {
    }

    void begin()
    {
        enableDWT();

        // clk pin, output, idle low
        configurePin(clkPort_, clkPin_, GPIO_MODE_OUTPUT_PP);
        clkPort_->BSRR = (static_cast<uint32_t>(clkPin_) << 16); // clk = LOW

        // data port, output initially, idle low
        setDataPortOutput();
        dataPort_->BSRR = (static_cast<uint32_t>(dataMask_) << 16); // data = LOW
    }

    // runtime
    void setClkHalfPeriod(uint32_t us) { clkHalfPeriodUs_ = us; }
    void setTurnaround(uint32_t us) { turnaroundUs_ = us; }

protected:
    // set data por to input
    void setDataPortOutput() override
    {
        setPortMode(dataPort_, dataMask_, GPIO_MODE_OUTPUT_PP);
    }
    // set dta port to input
    // External pull-downs on DATA lines prevent floating reads on absent slaves.
    void setDataPortInput() override
    {
        setPortMode(dataPort_, dataMask_, GPIO_MODE_INPUT);
    }

    void writeDataPort(uint16_t value) override
    {
        // Atomically set/clear only the masked pins using BSRR.
        // High bits: set pins where value bit = 1 AND in mask.
        // Low bits:  clear pins where value bit = 0 AND in mask.
        const uint16_t setMask = value & dataMask_;
        const uint16_t clearMask = (~value) & dataMask_;
        dataPort_->BSRR = static_cast<uint32_t>(setMask) |
                          (static_cast<uint32_t>(clearMask) << 16);
    }

    uint16_t readDataPort() override
    {
        return static_cast<uint16_t>(dataPort_->IDR) & dataMask_;
    }

    void setClk(bool high) override
    {
        if (high)
        {
            clkPort_->BSRR = clkPin_; // Set pin high
        }
        else
        {
            clkPort_->BSRR = static_cast<uint32_t>(clkPin_) << 16; // Set pin low
        }
    }

    void waitHalfPeriod() override
    {
        delayUs(clkHalfPeriodUs_);
    }

    void waitTurnaround() override
    {
        delayUs(turnaroundUs_);
    }

    /*Monotonic microsecond counter using DWT cycle counter.
    Returns DWT->CYCCNT divided by CPU MHz (168 for F407 at max speed).
    Wraps every ~25 seconds — safe because the base class poll loop uses
    unsigned subtraction: (currentUs() - readyStart) handles wrap correctly.*/
    uint32_t currentUs() override
    {
        return DWT->CYCCNT / (SystemCoreClock / 1000000UL);
    }

private:
    GPIO_TypeDef *dataPort_;
    uint16_t dataMask_;
    GPIO_TypeDef *clkPort_;
    uint16_t clkPin_;
    uint32_t clkHalfPeriodUs_;
    uint32_t turnaroundUs_;

    static void enableDWT()
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable DWT
        DWT->CYCCNT = 0;                                // Reset cycle counter
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable cycle
    }

    inline void delayUs(uint32_t us)
    {
        const uint32_t start = DWT->CYCCNT;
        const uint32_t ticks = (SystemCoreClock / 1000000UL);
        while ((DWT->CYCCNT - start) < ticks)
        {
            __NOP();
        }
    }

    static void configurePin(GPIO_TypeDef *port, uint16_t pinMask, uint32_t mode)
    {
        GPIO_InitTypeDef cfg = {};
        cfg.Pin = pinMask;
        cfg.Mode = mode;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // for 1mhz bit bang
        HAL_GPIO_Init(port, &cfg);
    }

    static void setPortMode(GPIO_TypeDef *port, uint16_t pinMask, uint32_t mode)
    {
        GPIO_InitTypeDef cfg = {};
        cfg.Pin = pinMask;
        cfg.Mode = mode;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // for 1mhz bit bang
        HAL_GPIO_Init(port, &cfg);
    }
};

#endif // STM32F4xx
#endif // SMARTDRIVE_STM32PARALLELCDNCTRANSPORT_H
