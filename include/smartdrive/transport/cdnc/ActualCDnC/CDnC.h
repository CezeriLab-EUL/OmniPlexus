#ifndef CDNC_H
#define CDNC_H

#include <stdint.h>
#include <stdbool.h>

#define CDNC_NUM_SLAVES 16
#define CDNC_RX_BUF_SIZE 128 // per slave — must fit MAX_FRAME_SIZE (67) with margin

typedef enum
{
    CDNC_SLAVE_OFFLINE = 0,
    CDNC_SLAVE_ONLINE = 1,
    CDNC_SLAVE_WAKING = 2,
} cdnc_slave_state_t;

#ifdef __cplusplus
extern "C"
{
#endif

    // Lifecycle
    void cdnc_init(void);
    uint16_t cdnc_exchange(void); // run one 18-cycle exchange; returns valid mask

    // TX byte pipe (called by CDnCTransport::send)
    bool cdnc_send_byte(uint8_t slave, uint8_t b);

    // RX byte pipe (called by CDnCTransport::accumulate)
    bool cdnc_recv_byte(uint8_t slave, uint8_t *out);
    uint8_t cdnc_rx_available(uint8_t slave);

    // Device detection
    cdnc_slave_state_t cdnc_slave_state_get(uint8_t slave);
    bool cdnc_slave_alive(uint8_t slave);
    uint16_t cdnc_alive_mask(void);

#ifdef __cplusplus
}
#endif

#endif // CDNC_H