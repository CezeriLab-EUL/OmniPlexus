
// CDnC drives all 16 slave data lines simultaneously via GPIOE in one write.
// One cdnc_exchange() call per ~5ms gap produces one byte TX/RX per slave.
//
// IMPORTANT: cdnc_exchange() must be called from the main firmware loop
// independently of TransportManager::listen(). The exchange loop drives the
// physical bus; listen() only drains the software RX ring buffers.

#if defined(CDNC_MASTER) || defined(CDNC_SLAVE)
#define CDNC_ENABLED
#endif

#pragma once
#include <stdint.h>
#include <stdbool.h>

//  Buffer sizes 
#define CDNC_MAX_SLAVES       16
#define CDNC_TX_BUF_SIZE     256
#define CDNC_RX_BUF_SIZE     128
#define CDNC_GAP_US         5000
#define CDNC_SLAVE_BROADCAST 0xFF

//  Slave detection state 
typedef enum {
    CDNC_SLAVE_OFFLINE = 0,   // not enough valid frames in recent window
    CDNC_SLAVE_ONLINE  = 1,   // ≥6 of last 8 frames valid
    CDNC_SLAVE_WAKING  = 2,   // transient: saw silent-then-present restart signature
} cdnc_slave_state_t;

//  Public API (implemented in CDnC.cpp, master-side only) 
#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
// Initialise GPIO, zero all buffers, and hold clock idle for 100ms.
// Call once at firmware startup before the exchange loop begins.
void               cdnc_init();

//TX Queue
// Enqueue one byte for transmission to the given slave on the next exchange(s).
// Bytes are written into the shared bit-plane TX ring buffer; this slave's
// bit position is OR/AND-masked so other slaves' queued data is undisturbed.
//
// Returns true on success, false if the queue would overflow (non-blocking).
// Overflow policy: caller must retry or drop; cdnc_send_byte never blocks.
bool               cdnc_send_byte(uint8_t slave, uint8_t b);

//RX Queue
// Dequeue one received byte for the given slave.
// Returns true and writes to *out if a byte is available; false if queue empty.
bool               cdnc_recv_byte(uint8_t slave, uint8_t* out);

// Returns the number of bytes currently available in the RX queue for slave.
// Saturates at 255.
uint8_t            cdnc_rx_available(uint8_t slave);

// Perform one 18-clock half-duplex exchange with all 16 slaves simultaneously:
//   cycle  1     : read start bit from all slaves
//   cycles 2–9   : transmit one byte per slave (from TX ring buffer)
//   cycles 10–17 : receive one byte per slave (into RX ring buffer)
//   cycle  18    : read end bit from all slaves
//
// After the exchange, pads any slave TX queues that have fallen behind
// and updates per-slave detection history.
//
// Returns a bitmask: bit N set = slave N had valid start AND end bits.
//
// MUST be called from the main firmware loop, not from inside listen().
// Typical call rate: once per ~5ms (GAP_US inter-frame gap).
uint16_t           cdnc_exchange();
uint32_t cdnc_write_ptr(uint8_t slave);  // returns writePtr[slave]
uint32_t cdnc_read_ptr();

// Device Detection
// Returns true if the slave is currently ONLINE (≥6/8 recent frames valid).
bool               cdnc_slave_alive(uint8_t slave);

// Returns a 16-bit mask: bit N set if slave N is ONLINE.
uint16_t           cdnc_alive_mask();

// Returns the full state enum for a slave (OFFLINE / ONLINE / WAKING).
cdnc_slave_state_t cdnc_slave_state_get(uint8_t slave);

#ifdef __cplusplus
}
#endif
