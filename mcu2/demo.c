#include <avr/pgmspace.h>
#include <stdio.h>

#include "iinic.h"

static uint8_t buffer[256];

void iinic_main()
{
    iinic_usart_is_debug();
    printf_P(PSTR("Hello, world!\n"));

    iinic_set_buffer(buffer, sizeof(buffer));
    for(;;) {
        iinic_rx();
        iinic_infinite_poll(IINIC_RX_COMPLETE);
        printf_P(PSTR("recv len = %d\n"), iinic_buffer_ptr - iinic_buffer);
    }
}
