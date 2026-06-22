// CDnC.cpp
// CDnC half-duplex parallel transport — physical layer implementation.
//
// Supports:
//   STM32F4xx — 16 data lines on GPIOE, clock on PA8
//   STM32F1xx — 16 data lines on GPIOB, clock on PA8 (Blue Pill)
//               Use setCdncSlotMask(0x001B) to restrict to slots 0,1,3,4
//               since PB2 = BOOT1 is not broken out on Blue Pill.
//
// Key design decisions (do not revert):
//   • txBuf initialised to 0xFFFF (not 0) — ODR=1 means release in open-drain
//   • NOP pad uses |= set_mask (release bit HIGH), not &= clear_mask (drive LOW)
//   • valid = (~start_bits) & (~end_bits) — LOW = valid, HIGH = absent
//   • cdnc_gpio_rx_mode sets PUPDR=0 — no internal pull-downs (they fight 5V pull-ups)
//   • GPIOE clock enabled BEFORE register writes (silently ignored otherwise)
//   • JTAG disabled on F103 to free PB3/PB4/PA15 for GPIO

#if defined(STM32F4xx) || defined(STM32F1xx)

#include "CDnC.h"
#include <Arduino.h>

// ── Internal constants ────────────────────────────────────────────────────────
#define CDNC_BUF_MASK     (CDNC_TX_BUF_SIZE - 1)
#define CDNC_RX_BUF_MASK  (CDNC_RX_BUF_SIZE - 1)
#define CLK_HALF_US       200
#define DETECT_WINDOW     8
#define DETECT_THRESHOLD  6

// ── Platform GPIO abstraction ─────────────────────────────────────────────────
#ifdef STM32F4xx
#define CDNC_DATA_GPIO GPIOE
#elif defined(STM32F1xx)
#define CDNC_DATA_GPIO GPIOB
#endif

// ── Slot mask ─────────────────────────────────────────────────────────────────
uint16_t _cdncSlotMask = 0xFFFF;  // default: all 16 slots active

// ── Internal state ────────────────────────────────────────────────────────────
static uint8_t clkPin = PA8;

// TX ring buffer: bit N of each entry = slave N's data line value
static volatile uint16_t txBuf[CDNC_TX_BUF_SIZE];
static volatile uint32_t readPtr                   = 0;
static volatile uint32_t writePtr[CDNC_MAX_SLAVES] = {0};

// RX ring buffers (one per slave)
static uint8_t  rxBuf[CDNC_MAX_SLAVES][CDNC_RX_BUF_SIZE];
static uint32_t rxWritePtr[CDNC_MAX_SLAVES] = {0};
static uint32_t rxReadPtr[CDNC_MAX_SLAVES]  = {0};

// Drop counters (diagnostic)
static volatile uint32_t cnt_rx_drops[CDNC_MAX_SLAVES] = {0};

// Detection state
static bool               saw_silent[CDNC_MAX_SLAVES]    = {false};
static uint16_t           valid_history[CDNC_MAX_SLAVES] = {0};
static cdnc_slave_state_t slave_state[CDNC_MAX_SLAVES];

// ── GPIO helpers ──────────────────────────────────────────────────────────────
static void cdnc_gpio_tx_mode(void) {
#ifdef STM32F4xx
    GPIOE->MODER   = 0x55555555;  // output mode on all 16 pins
    GPIOE->OTYPER  = 0xFFFF;      // open-drain on all 16 pins
    GPIOE->OSPEEDR = 0xAAAAAAAA;  // high speed
#elif defined(STM32F1xx)
    // CNF=01 (open-drain output), MODE=11 (50MHz output) → 0x7 per pin
    GPIOB->CRL = 0x77777777;      // PB0–PB7
    GPIOB->CRH = 0x77777777;      // PB8–PB15
#endif
}

static void cdnc_gpio_rx_mode(void) {
#ifdef STM32F4xx
    GPIOE->MODER = 0x00000000;    // input mode on all 16 pins
    GPIOE->PUPDR = 0x00000000;    // NO internal pull-downs — external 5V pull-ups only
    // Internal pull-downs (~40kΩ) cannot overcome 5V pull-up
    // and would falsely pull absent slots LOW
#elif defined(STM32F1xx)
    // CNF=01 (floating input), MODE=00 → 0x4 per pin
    GPIOB->CRL = 0x44444444;      // PB0–PB7 floating input
    GPIOB->CRH = 0x44444444;      // PB8–PB15 floating input
#endif
}

static void cdnc_clock_pulse(void) {
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(CLK_HALF_US);
    digitalWrite(clkPin, LOW);
    delayMicroseconds(CLK_HALF_US);
}

// ── Detection helpers ─────────────────────────────────────────────────────────
static uint8_t popcount_window(uint16_t v) {
    uint16_t masked = v & ((1U << DETECT_WINDOW) - 1);
    uint8_t count = 0;
    while (masked) { count += masked & 1; masked >>= 1; }
    return count;
}

static void cdnc_update_detection(uint16_t valid_mask) {
    valid_mask &= _cdncSlotMask;  // force unconnected slots to always-invalid

    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        uint8_t bit = (valid_mask >> s) & 1;
        valid_history[s] = (valid_history[s] << 1) | bit;
        uint8_t recent = popcount_window(valid_history[s]);

        cdnc_slave_state_t prev = slave_state[s];
        cdnc_slave_state_t next = prev;

        if (saw_silent[s] && bit == 1) {
            // Was silent (disconnected), now valid again — waking up
            next = CDNC_SLAVE_WAKING;
            saw_silent[s] = false;
        } else if (prev == CDNC_SLAVE_WAKING) {
            // One exchange grace period — now confirm ONLINE
            next = CDNC_SLAVE_ONLINE;
        } else {
            // Sliding window: ≥6/8 recent exchanges valid = ONLINE
            next = (recent >= DETECT_THRESHOLD)
                       ? CDNC_SLAVE_ONLINE : CDNC_SLAVE_OFFLINE;
            // Mark saw_silent when online slave misses an exchange
            if (prev == CDNC_SLAVE_ONLINE && bit == 0) {
                saw_silent[s] = true;
            }
            if (next == CDNC_SLAVE_OFFLINE) saw_silent[s] = false;
        }

        slave_state[s] = next;
    }
}

// ── RX enqueue ────────────────────────────────────────────────────────────────
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

// ── TX pad ────────────────────────────────────────────────────────────────────
// When a slave's TX queue falls behind the read pointer, fill with 1s (release).
// CRITICAL: must use |= set_mask (ODR=1 = release in open-drain),
//           NOT &= clear_mask (ODR=0 = actively drive LOW, corrupts slave RX).
static void cdnc_post_exchange_pad(void) {
    uint32_t r = readPtr;
    for (uint8_t s = 0; s < CDNC_MAX_SLAVES; s++) {
        if (writePtr[s] <= r) {
            uint16_t set_mask = (uint16_t)1 << s;
            uint32_t w = writePtr[s];
            for (int i = 0; i < 8; i++) {
                txBuf[(w + i) & CDNC_BUF_MASK] |= set_mask;  // release = HIGH
            }
            writePtr[s] = w + 8;
        }
    }
}

uint32_t cdnc_write_ptr(uint8_t slave) { return writePtr[slave]; }
uint32_t cdnc_read_ptr(void)           { return readPtr; }

// ── Public API ────────────────────────────────────────────────────────────────

void cdnc_init(void) {
#ifdef STM32F4xx
    // CRITICAL: enable clock BEFORE any register writes — writes before
    // clock enable are silently ignored, leaving open-drain unconfigured
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIOE->MODER  = 0x55555555;   // output mode
    GPIOE->OTYPER = 0xFFFF;       // open-drain on all 16 pins
    GPIOE->PUPDR  = 0x00000000;   // no pull — external 5V pull-ups only

#elif defined(STM32F1xx)
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    // Disable JTAG, keep SWD — frees PB3, PB4, PA15 for GPIO use
    // Required on Blue Pill since PB3/PB4 are JTDO/JTRST by default
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG)
                 | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    // CNF=01 open-drain output, MODE=11 50MHz → 0x7 per pin
    GPIOB->CRL = 0x77777777;      // PB0–PB7
    GPIOB->CRH = 0x77777777;      // PB8–PB15
#endif

    // CLK pin: open-drain output, idle LOW
    pinMode(clkPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(clkPin, LOW);

    // TX buffer: all 1s = all lines released (open-drain HIGH via pull-up)
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

    delay(100);  // master-restart gap: slaves detecting >100ms clock absence reset
}

void setCdncSlotMask(uint16_t mask) {
    _cdncSlotMask = mask;
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

bool cdnc_recv_byte(uint8_t slave, uint8_t* out) {
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

    // Cycle 1: read start bit (connected slave drives LOW)
    cdnc_gpio_rx_mode();
    delayMicroseconds(50);
    start_bits = CDNC_DATA_GPIO->IDR & 0xFFFF;
    cdnc_clock_pulse();

    // Cycles 2–9: TX byte from queue (master drives, slaves read)
    cdnc_gpio_tx_mode();

    for (int i = 0; i < 8; i++) {
        uint16_t plane = txBuf[(readPtr + i) & CDNC_BUF_MASK];
        CDNC_DATA_GPIO->ODR = plane;
        delayMicroseconds(20);

        if (i == 7) {
            // Asymmetric pulse on cycle 9:
            // Master raises CLK, switches to RX, lowers CLK.
            // Slave drives bit 7 of its response during the 370µs gap
            // before the master samples cycle 10.
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
        rx_planes[i] = CDNC_DATA_GPIO->IDR & 0xFFFF;
        cdnc_clock_pulse();
    }

    // Cycle 18: read end bit (connected slave drives LOW)
    delayMicroseconds(20);
    end_bits = CDNC_DATA_GPIO->IDR & 0xFFFF;
    cdnc_clock_pulse();

    // Valid detection: LOW = slave present, HIGH = absent (floats via pull-up)
    // Invert so that bit=1 means "this slot had a valid exchange"
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
    return mask & _cdncSlotMask;
}

cdnc_slave_state_t cdnc_slave_state_get(uint8_t slave) {
    if (slave >= CDNC_MAX_SLAVES) return CDNC_SLAVE_OFFLINE;
    return slave_state[slave];
}

#endif // STM32F4xx || STM32F1xx
