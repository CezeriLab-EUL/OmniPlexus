// CDnCConfig.h
// Shared CDnC constants and API declarations.
// Included by both master (CDnC.h) and slave (CDnCSlaveTransport.h).
//
// IMPORTANT: cdnc_exchange() must be called from the main firmware loop
// independently of TransportManager::listen(). The exchange loop drives the
// physical bus; listen() only drains the software RX ring buffers.

#pragma once
#include <stdbool.h>
#include <stdint.h>

// ── Buffer sizes
// ──────────────────────────────────────────────────────────────
#define CDNC_MAX_SLAVES 16
#define CDNC_TX_BUF_SIZE 256
#define CDNC_RX_BUF_SIZE 128
#define CDNC_GAP_US 5000
#define CDNC_SLAVE_BROADCAST 0xFF

// ── Slot mask
// ───────────────────────────────────────────────────────────────── Only slots
// with their bit set are treated as active. Unset slots are forced OFFLINE
// regardless of line state. Default 0xFFFF = all 16 slots active. Blue Pill (no
// PB2/BOOT1): use 0x001B (bits 0,1,3,4).
extern uint16_t _cdncSlotMask;

// ── Slave detection state
// ─────────────────────────────────────────────────────
typedef enum {
  CDNC_SLAVE_OFFLINE = 0, // not enough valid frames in recent window
  CDNC_SLAVE_ONLINE = 1,  // ≥6 of last 8 frames valid
  CDNC_SLAVE_WAKING = 2,  // transient: saw silent-then-present restart
} cdnc_slave_state_t;

// ── Public API
// ────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
void cdnc_init();
void setCdncSlotMask(uint16_t mask);

// TX queue
bool cdnc_send_byte(uint8_t slave, uint8_t b);

// RX queue
bool cdnc_recv_byte(uint8_t slave, uint8_t *out);
uint8_t cdnc_rx_available(uint8_t slave);

// Bus exchange — call every loop iteration from main loop
uint16_t cdnc_exchange();

// Diagnostics
uint32_t cdnc_write_ptr(uint8_t slave);
uint32_t cdnc_read_ptr();

// Detection
bool cdnc_slave_alive(uint8_t slave);
uint16_t cdnc_alive_mask();
cdnc_slave_state_t cdnc_slave_state_get(uint8_t slave);

#ifdef __cplusplus
}
#endif
