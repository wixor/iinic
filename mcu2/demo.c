#include <avr/pgmspace.h>
#include <stdio.h>

#include "iinic.h"

static uint8_t buffer[256];

void iinic_main()
{
    iinic_usart_is_debug();
    printf_P(PSTR("Hello, world!\r\n"));

    for(;;) {
        iinic_set_buffer(buffer, sizeof(buffer));
        iinic_rx();
        iinic_infinite_poll(IINIC_RX_COMPLETE);

        iinic_timing later = iinic_rx_timing;
        iinic_timing_add_32(&later, 36864);
        iinic_timed_poll(0, &later);

        uint16_t sz = iinic_buffer_ptr - iinic_buffer;
        iinic_set_buffer(buffer, sz);
        iinic_tx();
        printf_P(PSTR("recv len = %d, rssi = %d, timing = %02x%02x%02x%02x%02x%02x\r\n"),
            sz, iinic_rx_rssi, iinic_rx_timing.b[5],iinic_rx_timing.b[4],iinic_rx_timing.b[3],iinic_rx_timing.b[2],iinic_rx_timing.b[1],iinic_rx_timing.b[0]);
        iinic_infinite_poll(IINIC_TX_COMPLETE);
    }
}
