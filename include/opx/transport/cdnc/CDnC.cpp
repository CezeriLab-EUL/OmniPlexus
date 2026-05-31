// CDnC.cpp
// Implementation of the CDnC half-duplex parallel transport layer.

#ifdef STM32F4xx

#include "CDnC.h"
#include <Arduino.h>

//  Internal constants

#define CDNC_BUF_MASK        (CDNC_TX_BUF_SIZE - 1)
#define CDNC_RX_BUF_MASK     (CDNC_RX_BUF_SIZE - 1)
#define CLK_HALF_US          200
#define DETECT_WINDOW        8
#define DETECT_THRESHOLD     6

//  Internal state

static uint8_t  clkPin = PA8;

// TX ring buffer (bit-plane: bit N of each slot = slave N's data line)
static volatile uint16_t txBuf[CDNC_TX_BUF_SIZE];
static volatile uint32_t readPtr               = 0;
static volatile uint32_t writePtr[CDNC_MAX_SLAVES] = {0};

// RX ring buffers (one per slave)
static uint8_t  rxBuf[CDNC_MAX_SLAVES][CDNC_RX_BUF_SIZE];
static uint32_t rxWritePtr[CDNC_MAX_SLAVES] = {0};
static uint32_t rxReadPtr[CDNC_MAX_SLAVES]  = {0};

// Drop counters (diagnostic)
static volatile uint32_t cnt_rx_drops[CDNC_MAX_SLAVES] = {0};

// Detection state
static bool              saw_silent[CDNC_MAX_SLAVES]     = {false};
static uint16_t          valid_history[CDNC_MAX_SLAVES]  = {0};
static cdnc_slave_state_t slave_state[CDNC_MAX_SLAVES];

//  GPIO helpers (unchanged from .ino)

static void cdnc_gpio_tx_mode(void) {
    // GPIOE->MODER   = 0x55555555;   // all 16 pins → output
    // GPIOE->OSPEEDR = 0xAAAAAAAA;   // high speed

    // Configure all 16 data pins as open-drain outputs
    GPIOE->MODER = 0x55555555;   // Output mode
    GPIOE->OTYPER = 0xFFFF;      // Open-drain on ALL pins
    // GPIOE->PUPDR = 0x00000000;   // No pull-up/down
    GPIOE->OSPEEDR = 0xAAAAAAAA; // High speed
}

static void cdnc_gpio_rx_mode(void) {
    // GPIOE->MODER = 0x00000000;     // all 16 → input
    // GPIOE->PUPDR = 0xAAAAAAAA;     // pull-down on all 16

    GPIOE->MODER = 0x00000000;   // Input mode
    GPIOE->PUPDR = 0x00000000;   // No pull-up/down
    // OTYPER doesn't matter in input modes
}

static void cdnc_clock_pulse(void) {
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(CLK_HALF_US);
    digitalWrite(clkPin, LOW);
    delayMicroseconds(CLK_HALF_US);
}

//  Detection helpers (unchanged from .ino)

static uint8_t popcount_window(uint16_t v) {
    uint16_t masked = v & ((1U << DETECT_WINDOW) - 1);
    uint8_t count = 0;
    while (masked) {
        count += masked & 1;
        masked >>= 1;
    }
    return count;
}

static void cdnc_update_detection(uint16_t valid_mask) {
    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        uint8_t bit = (valid_mask >> s) & 1;
        valid_history[s] = (valid_history[s] << 1) | bit;
        uint8_t recent = popcount_window(valid_history[s]);

        cdnc_slave_state_t prev = slave_state[s];
        cdnc_slave_state_t next = prev;

        if (prev == CDNC_SLAVE_ONLINE && bit == 0) {
            saw_silent[s] = true;
        } else if (saw_silent[s] && bit == 1) {
            next = CDNC_SLAVE_WAKING;
            saw_silent[s] = false;
        } else if (prev == CDNC_SLAVE_WAKING) {
            next = CDNC_SLAVE_ONLINE;
        } else {
            next = (recent >= DETECT_THRESHOLD) ? CDNC_SLAVE_ONLINE : CDNC_SLAVE_OFFLINE;
            if (next == CDNC_SLAVE_OFFLINE) saw_silent[s] = false;
        }

        slave_state[s] = next;
    }
}

//  RX enqueue (internal, unchanged from .ino)

static void cdnc_rx_enqueue(uint8_t slave, uint8_t b) {
    uint32_t w = rxWritePtr[slave];
    uint32_t r = rxReadPtr[slave];

    if ((w - r) >= CDNC_RX_BUF_SIZE) {
        cnt_rx_drops[slave]++;
        rxReadPtr[slave] = w - CDNC_RX_BUF_SIZE + 1;
    }

    rxBuf[slave][w & CDNC_RX_BUF_MASK] = b;
    rxWritePtr[slave] = w + 1;
}

//  TX pad (internal, unchanged from .ino)

static void cdnc_post_exchange_pad(void) {
    uint32_t r = readPtr;
    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        if (writePtr[s] <= r) {
            uint16_t set_mask   = (uint16_t)1 << s;
            uint16_t clear_mask = ~((uint16_t)1 << s);
            uint32_t w = writePtr[s];
            for (int i = 0; i < 8; i++) {
                // txBuf[(w + i) & CDNC_BUF_MASK] &= clear_mask;
                txBuf[(w + i) & CDNC_BUF_MASK] |= set_mask;

            }
            writePtr[s] = w + 8;
        }
    }
}

uint32_t cdnc_write_ptr(uint8_t slave) { return writePtr[slave];}
uint32_t cdnc_read_ptr(void) { return readPtr; }

//  Public API

void cdnc_init(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();

    pinMode(clkPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(clkPin, LOW);

    GPIOE->MODER = 0x55555555;   // Output mode
    GPIOE->OTYPER = 0xFFFF;      // Open-drain on ALL pins
    GPIOE->PUPDR  = 0x00000000;
    GPIOE->PUPDR = 0xAAAAAAAA;  // pull-down on all 16 pins

    // for (int i = 0; i < CDNC_TX_BUF_SIZE; i++) txBuf[i] = 0;
    for (int i = 0; i < CDNC_TX_BUF_SIZE; i++) txBuf[i] = 0xFFFF;

    readPtr = 0;
    for (int s = 0; s < CDNC_MAX_SLAVES; s++) writePtr[s] = 0;

    for (int s = 0; s < CDNC_MAX_SLAVES; s++) {
        rxWritePtr[s] = 0;
        rxReadPtr[s]  = 0;
    }

    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        valid_history[s] = 0;
        slave_state[s]   = CDNC_SLAVE_OFFLINE;
        saw_silent[s]    = false;
    }

    delay(100);   // master-restart wake: slaves detecting >100ms gap reset
}

bool cdnc_send_byte(uint8_t slave, uint8_t b) {
    if (slave >= CDNC_MAX_SLAVES) return false;

    uint32_t w = writePtr[slave];
    uint32_t r = readPtr;

    if ((w - r) + 8 > CDNC_TX_BUF_SIZE) return false;

    uint16_t set_mask   = (uint16_t)1 << slave;
    uint16_t clear_mask = ~set_mask;

    for (int bit = 7; bit >= 0; bit--) {
        uint32_t idx = (w + (7 - bit)) & CDNC_BUF_MASK;
        if ((b >> bit) & 1) txBuf[idx] |=  set_mask;
        else                txBuf[idx] &= clear_mask;
    }

    writePtr[slave] = w + 8;
    return true;
}

bool cdnc_recv_byte(uint8_t slave, uint8_t *out) {
    if (slave >= CDNC_MAX_SLAVES) return false;
    uint32_t r = rxReadPtr[slave];
    uint32_t w = rxWritePtr[slave];
    if (r == w) return false;
    *out = rxBuf[slave][r & CDNC_RX_BUF_MASK];
    rxReadPtr[slave] = r + 1;
    return true;
}

uint8_t cdnc_rx_available(uint8_t slave) {
    if (slave >= CDNC_MAX_SLAVES) return 0;
    uint32_t avail = rxWritePtr[slave] - rxReadPtr[slave];
    return (avail > 255) ? 255 : (uint8_t)avail;
}

uint16_t cdnc_exchange(void) {
    uint16_t start_bits, end_bits;
    uint16_t rx_planes[8];

    // Cycle 1: read start bit
    cdnc_gpio_rx_mode();
    delayMicroseconds(50);
    start_bits = GPIOE->IDR & 0xFFFF;
    cdnc_clock_pulse();

    // Cycles 2–9: TX byte from queue
    cdnc_gpio_tx_mode();
    // GPIOE->PUPDR = 0;

    for (int i = 0; i < 8; i++) {
        uint16_t plane = txBuf[(readPtr + i) & CDNC_BUF_MASK];
        GPIOE->ODR = plane;
        delayMicroseconds(20);

        if (i == 7) {
            // Asymmetric pulse on cycle 9: fixes bit-0 read & avoids contention
            digitalWrite(clkPin, HIGH);
            delayMicroseconds(30);
            cdnc_gpio_rx_mode();
            digitalWrite(clkPin, LOW);
            delayMicroseconds(370);
        } else {
            cdnc_clock_pulse();
        }
    }

    readPtr += 8;

    // Cycles 10–17: RX byte from each slave
    for (int i = 0; i < 8; i++) {
        delayMicroseconds(20);
        rx_planes[i] = GPIOE->IDR & 0xFFFF;
        cdnc_clock_pulse();
    }

    // Cycle 18: read end bit
    delayMicroseconds(20);
    end_bits = GPIOE->IDR & 0xFFFF;
    cdnc_clock_pulse();

    // uint16_t valid = start_bits & end_bits;
    uint16_t valid = (~start_bits) & (~end_bits);

    // De-interleave RX bit-planes into per-slave RX ring buffers
    for (int s = 0; s < CDNC_MAX_SLAVES; s++) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            byte |= ((rx_planes[bit] >> s) & 1) << (7 - bit);
        }
        cdnc_rx_enqueue(s, byte);
    }

    cdnc_post_exchange_pad();
    cdnc_update_detection(valid);
    return valid;
}

bool cdnc_slave_alive(uint8_t slave) {
    return cdnc_slave_state_get(slave) == CDNC_SLAVE_ONLINE;
}

uint16_t cdnc_alive_mask(void) {
    uint16_t mask = 0;
    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        if (slave_state[s] == CDNC_SLAVE_ONLINE) mask |= (1U << s);
    }
    return mask;
}

cdnc_slave_state_t cdnc_slave_state_get(uint8_t slave) {
    if (slave >= CDNC_MAX_SLAVES) return CDNC_SLAVE_OFFLINE;
    return slave_state[slave];
}

#endif // STM32F4xx
