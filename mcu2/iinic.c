#include <stdio.h> /* yes, avr has stdio! */
#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "iinic.h"

#define must_read(x) (*(const volatile typeof(x) *)&x)
#define must_write(x) (*(volatile typeof(x) *)&x)

static inline __attribute__((always_inline)) void nop() {
    asm volatile("nop\n\t");
}

static void panic() __attribute__((noreturn));

/* ------------------------------------------------------------------------- */

/* written by TIMER1_OVF_vect,
 * read by main and INT0_vect,
 * but only via asm code */
static volatile uint32_t timing_high;

ISR(TIMER1_OVF_vect, ISR_NAKED)
{
    /* this does
     *     timing_high += 1;
     * but is more efficient than gcc's code
     */
    asm volatile(
        "push r24\n\t"
        "in r24, __SREG__\n\t"
        "push r24\n\t"

        "lds r24, timing_high\n\t"
        "inc r24\n\t"
        "sts timing_high, r24\n\t"
        "brne 1f\n\t"

        "lds r24, timing_high+1\n\t"
        "inc r24\n\t"
        "sts timing_high+1, r24\n\t"
        "brne 1f\n\t"

        "lds r24, timing_high+2\n\t"
        "inc r24\n\t"
        "sts timing_high+2, r24\n\t"
        "brne 1f\n\t"

        "lds r24, timing_high+3\n\t"
        "inc r24\n\t"
        "sts timing_high+3, r24\n\t"

"1:\n\t"
        "pop r24\n\t"
        "out __SREG__, r24\n\t"
        "pop r24\n\t"
        "reti"
    );
}

void iinic_get_now(iinic_timing *out)
{
    uint8_t t0, t1;

    asm volatile(
     "1: lds  %1, timing_high+0\n\t"
        "std  %a2+2, %1\n\t"
        "lds  %0, timing_high+1\n\t"
        "std  %a2+3, %0\n\t"
        "lds  %0, timing_high+2\n\t"
        "std  %a2+4, %0\n\t"
        "lds  %0, timing_high+3\n\t"
        "std  %a2+5, %0\n\t"
        "in   %0, 0x2c\n\t" // TCNT1L
        "std  %a2+0, %0\n\t"
        "in   %0, 0x2d\n\t" // TCNT1H
        "std  %a2+1, %0\n\t"
        "lds  %0, timing_high+0\n\t"
        "cp   %0, %1\n\t"
        "brne 1b\n\t"
        : "=&a" (t0), "=&a" (t1)
        : "b" (out)
    );
}

int8_t iinic_now_cmp(const iinic_timing *ref)
{
    int8_t t0, t1, t2;

    asm volatile(
     "1: lds  %2, timing_high+0\n\t"

        "in   %0, 0x2c\n\t" // TCNT1L
        "ldd  %1, %a3+0\n\t"
        "cp   %0, %1\n\t"

        "in   %0, 0x2d\n\t" // TCNT1H
        "ldd  %1, %a3+1\n\t"
        "cpc  %0, %1\n\t"

        "ldd  %1, %a3+2\n\t"
        "cpc  %2, %1\n\t"

        "lds  %0, timing_high+1\n\t"
        "ldd  %1, %a3+3\n\t"
        "cpc  %0, %1\n\t"

        "lds  %0, timing_high+2\n\t"
        "ldd  %1, %a3+4\n\t"
        "cpc  %0, %1\n\t"

        "lds  %0, timing_high+3\n\t"
        "ldd  %1, %a3+5\n\t"
        "cpc  %0, %1\n\t"

        "ldi %0, 0\n\t"
        "breq 2f\n\t"
        "ldi %0, 0xff\n\t"
        "brlo 2f\n\t"
        "ldi %0, 1\n\t"

     "2: lds  %1, timing_high+0\n\t"
        "cp   %1, %2\n\t"
        "brne 1b\n\t"

        : "=&a" (t0), "=&a" (t1), "=&a" (t2)
        : "b" (ref)
    );
    return t0;
}

void iinic_timing_add_32(iinic_timing *a, int32_t b)
{
    uint8_t t0, t1;
    asm volatile(
        "ldd %0, %a3+0\n\t"
        "add %0, %A2\n\t"
        "std %a3+0, %0\n\t"

        "ldd %0, %a3+1\n\t"
        "adc %0, %B2\n\t"
        "std %a3+1, %0\n\t"

        "ldd %0, %a3+2\n\t"
        "adc %0, %C2\n\t"
        "std %a3+2, %0\n\t"

        "ldd %0, %a3+3\n\t"
        "adc %0, %D2\n\t"
        "std %a3+3, %0\n\t"

        "mov %1, %D2\n\t"
        "rol %1\n\t"
        "sbc %1, %1\n\t"

        "ldd %0, %a3+4\n\t"
        "adc %0, %1\n\t"
        "std %a3+4, %0\n\t"

        "ldd %0, %a3+5\n\t"
        "adc %0, %1\n\t"
        "std %a3+5, %0\n\t"

        : "=&a" (t0), "=&a" (t1)
        : "a" (b), "b" (a)
    );
}

void iinic_timing_add(iinic_timing *a, const iinic_timing *b)
{
    uint8_t t0, t1;
    asm volatile(
        "ldd %0, %a2+0\n\t"
        "ldd %1, %a3+0\n\t"
        "add %0, %1\n\t"
        "std %a2+0, %0\n\t"

        "ldd %0, %a2+1\n\t"
        "ldd %1, %a3+1\n\t"
        "adc %0, %1\n\t"
        "std %a2+1, %0\n\t"

        "ldd %0, %a2+2\n\t"
        "ldd %1, %a3+2\n\t"
        "adc %0, %1\n\t"
        "std %a2+2, %0\n\t"

        "ldd %0, %a2+3\n\t"
        "ldd %1, %a3+3\n\t"
        "adc %0, %1\n\t"
        "std %a2+3, %0\n\t"

        "ldd %0, %a2+4\n\t"
        "ldd %1, %a3+4\n\t"
        "adc %0, %1\n\t"
        "std %a2+4, %0\n\t"

        "ldd %0, %a2+5\n\t"
        "ldd %1, %a3+5\n\t"
        "adc %0, %1\n\t"
        "std %a2+5, %0\n\t"

        : "=&a" (t0), "=&a" (t1)
        : "b" (a), "b" (b)
    );
}

void iinic_timing_sub(iinic_timing *a, const iinic_timing *b)
{
    uint8_t t0, t1;
    asm volatile(
        "ldd %0, %a2+0\n\t"
        "ldd %1, %a3+0\n\t"
        "sub %0, %1\n\t"
        "std %a2+0, %0\n\t"

        "ldd %0, %a2+1\n\t"
        "ldd %1, %a3+1\n\t"
        "sbc %0, %1\n\t"
        "std %a2+1, %0\n\t"

        "ldd %0, %a2+2\n\t"
        "ldd %1, %a3+2\n\t"
        "sbc %0, %1\n\t"
        "std %a2+2, %0\n\t"

        "ldd %0, %a2+3\n\t"
        "ldd %1, %a3+3\n\t"
        "sbc %0, %1\n\t"
        "std %a2+3, %0\n\t"

        "ldd %0, %a2+4\n\t"
        "ldd %1, %a3+4\n\t"
        "sbc %0, %1\n\t"
        "std %a2+4, %0\n\t"

        "ldd %0, %a2+5\n\t"
        "ldd %1, %a3+5\n\t"
        "sbc %0, %1\n\t"
        "std %a2+5, %0\n\t"

        : "=&a" (t0), "=&a" (t1)
        : "b" (a), "b" (b)
    );
}

int8_t iinic_timing_cmp(const iinic_timing *a, const iinic_timing *b)
{
    int8_t t0, t1;
    asm volatile(
        "ldd %0, %a2+0\n\t"
        "ldd %1, %a3+0\n\t"
        "cp  %0, %1\n\t"

        "ldd %0, %a2+1\n\t"
        "ldd %1, %a3+1\n\t"
        "cpc %0, %1\n\t"

        "ldd %0, %a2+2\n\t"
        "ldd %1, %a3+2\n\t"
        "cpc %0, %1\n\t"

        "ldd %0, %a2+3\n\t"
        "ldd %1, %a3+3\n\t"
        "cpc %0, %1\n\t"

        "ldd %0, %a2+4\n\t"
        "ldd %1, %a3+4\n\t"
        "cpc %0, %1\n\t"

        "ldd %0, %a2+5\n\t"
        "ldd %1, %a3+5\n\t"
        "cpc %0, %1\n\t"

        "ldi %0, 0\n\t"
        "breq 1f\n\t"
        "ldi %0, 0xff\n\t"
        "brlo 1f\n\t"
        "ldi %0, 1\n\t"
     "1:\n\t"

        : "=&a" (t0), "=&a" (t1)
        : "b" (a), "b" (b)
    );

    return t0;
}

/* ------------------------------------------------------------------------- */

enum {
    /* radio status: high byte */
    RADIO_RGIT = 7, /* TX register is ready to receive the next byte */
    RADIO_FFIT = 7, /* The number of data bits in the RX FIFO has reached the pre-programmed limit */
    RADIO_POR  = 6, /* Power-on reset */
    RADIO_RGUR = 5, /* TX register underrun, register overwrite */
    RADIO_FFOV = 5, /* RX FIFO overflow */
    RADIO_WKUP = 4, /* Wake-up timer overflow */
    RADIO_EXT  = 3, /* Logic level on interrupt pin changed to low */
    RADIO_LBD  = 2, /* Low battery detected; the power supply voltage is beloe the pre-programmed limit */
    RADIO_FFEM = 1, /* FIFO is empty */
    RADIO_ATS  = 0, /* Antenna tuning circuit detected string enough RF signal */
    RADIO_RSSI = 0, /* The strenght of the incomming signal is above the pre-programmed limit */

    /* radio status: low byte */
    RADIO_DQD  = 7, /* Data quality detector output */
    RADIO_CRL  = 6, /* Clock recovery locked */
    RADIO_ATGL = 5, /* AFC cycle */

    /* sync bytes */
    RADIO_SYNC_BYTE1 = 0x2D,
    RADIO_SYNC_BYTE2 = 0xD4,

    /* default setup */
    DEFAULT_FREQ = 0x680, /* 868.32 MHz */
    DEFAULT_BITRATE = IINIC_BITRATE_9600,
    DEFAULT_RX_KNOBS = IINIC_RSSI_91 | IINIC_GAIN_20 | IINIC_BW_67,
    DEFAULT_TX_KNOBS = IINIC_POWER_175 | IINIC_DEVIATION_60,
};

/* ------- raw i/o -------  */

static inline void radio_begin() {
    /* set pin low */
    PORTB &=~ _BV(4);
    /* wait > 10ns */
    nop();
    nop();
}
static inline void radio_end() {
    /* set pin high */
    PORTB |= _BV(4);
    /* wait 25ns */
    nop();
    nop();
}
static uint8_t radio_io(uint8_t v)
{
    (void) SPSR; /* this + the next write clears spi interrupt flag */
    SPDR = v;
    while(!(SPSR & _BV(SPIF)));
    return SPDR;
}

static void radio_write(uint16_t v)
{
    radio_begin();
    radio_io(v >> 8);
    radio_io(v & 0xff);
    radio_end();
}
void __iinic_radio_write(uint16_t v) __attribute__ (( alias("radio_write") ));

static inline void radio_irq_disable() {
    GICR &= ~_BV(INT0);
}
static inline void radio_irq_enable() {
    GICR |= _BV(INT0);
}

/* ------- radio commands -------  */

static void radio_reset()
{
    /* nRST: in-pullup -> in-highz -> out-low */
    PORTB &=~ _BV(1);
    DDRB |= _BV(1);

    /* wait a while */
    nop(); nop(); nop(); nop();
    nop(); nop(); nop(); nop();

    /* nRST: out-low -> in-highz -> in-pullup */
    DDRB &=~ _BV(1);
    PORTB |= _BV(1);

    /* wait while radio completes reset */
    while(! (PINB & _BV(1)) )
        wdt_reset();

    /* wait for power-on-reset interrupt */
    do
        wdt_reset();
    while(PIND & _BV(2));

    /* read out power-on-reset status */
    radio_begin();
    uint8_t status_high = radio_io(0);
    radio_io(0);
    radio_end();

    if(status_high != _BV(RADIO_POR))
        panic();
}

static inline void radio_enable_rxfifo()
{
    /* 0xCA00 = fifo and reset mode command
     * f3:0 = 1000 (fifo interrupt threshold; 8 bits = 1 byte = default)
     * sp = 0 (two-byte synchronization pattern)
     * al = 0 (fifo fill start condition: on synchron pattern reception)
     * ff = 1 (fifo fill will be enabled after synchron pattern reception)
     * dr = 0 (sentensive reset mode)
     */
    radio_write(0xCA82);
}

static inline void radio_disable_rxfifo()
{
    /* 0xCA00 = fifo and reset mode command
     * f3:0 = 1000 (fifo interrupt threshold; 8 bits = 1 byte = default)
     * sp = 0 (two-byte synchronization pattern)
     * al = 0 (fifo fill start condition: on synchron pattern reception)
     * ff = 0 (fifo fill disabled)
     * dr = 0 (sentensive reset mode)
     */
    radio_write(0xCA80);
}

static inline void radio_mode_idle()
{
    radio_disable_rxfifo();

    /* 0x8000 == configuration setting command
     * el   = 0 (internal data register aka. tx buffer)
     * ef   = 0 (fifo mode aka. rx buffer)
     * b1:0 = 10 (868 MHz band)
     * x4:0 = 1000 (12.5pf xtal load capacitance)
     */
    radio_write(0x8028);

    /* 0x8200 == power management command
     * er  = 0 (rf front-end)
     * ebb = 1 (baseband)
     * et  = 0 (transmitter)
     * es  = 1 (synthesier)
     * ex  = 1 (crystal oscillattor)
     * eb  = 0 (low battery detector)
     * ew  = 0 (wake-up timer)
     * dc  = 1 (clock output disabled)
     */
    radio_write(0x82A9);
}

static inline void radio_mode_rx()
{
    /* 0x8000 == configuration setting command
     * el   = 0 (internal data register aka. tx buffer)
     * ef   = 1 (fifo mode aka. rx buffer)
     * b1:0 = 10 (868 MHz band)
     * x4:0 = 1000 (12.5pf xtal load capacitance)
     */
    radio_write(0x8068);

    /* 0x8200 == power management command
     * er  = 1 (rf front-end)
     * ebb = 1 (baseband)
     * et  = 0 (transmitter)
     * es  = 1 (synthesier)
     * ex  = 1 (crystal oscillattor)
     * eb  = 0 (low battery detector)
     * ew  = 0 (wake-up timer)
     * dc  = 1 (clock output disabled)
     */
    radio_write(0x82D9);

    radio_enable_rxfifo();
}

static inline void radio_mode_tx()
{
    radio_disable_rxfifo();

    /* 0x8000 == configuration setting command
     * el   = 1 (internal data register aka. tx buffer)
     * ef   = 0 (fifo mode aka. rx buffer)
     * b1:0 = 10 (868 MHz band)
     * x4:0 = 1000 (12.5pf xtal load capacitance)
     */
    radio_write(0x80A8);

    /* 0x8200 == power management command
     * er  = 0 (rf front-end)
     * ebb = 1 (baseband)
     * et  = 1 (transmitter)
     * es  = 1 (synthesier)
     * ex  = 1 (crystal oscillattor)
     * eb  = 0 (low battery detector)
     * ew  = 0 (wake-up timer)
     * dc  = 1 (clock output disabled)
     */
    radio_write(0x8279);
}

static inline void radio_set_data_filter()
{
    /* 0xC200 = data filter command
     * al = 1 (clock recovery in auto mode)
     * ml = 0 (manual clock recovery setting; meaningless)
     * s = 0 (digital data filter)
     * f2:0 = 100 (dqd threshold = 4)
     */
    radio_write(0xC2AC);
}


static inline void radio_tx(uint8_t v) {
    /* 0xB800 == tx register write command */
    radio_write(0xB800 | v);
}

/* ------------------------------------------------------------------------- */

uint8_t __iinic_state;
#define state __iinic_state

int32_t __iinic_rx_timing_offset;
#define rx_timing_offset __iinic_rx_timing_offset

static uint8_t tx_framing;

uint16_t iinic_mac;

uint8_t *iinic_buffer,
        *iinic_buffer_ptr,
        *iinic_buffer_end;

uint16_t iinic_rx_rssi;
iinic_timing iinic_rx_timing;

void iinic_set_buffer(uint8_t *buf, uint16_t len)
{
    if((IINIC_RX_IDLE | IINIC_RX_ACTIVE | IINIC_TX_ACTIVE) & state)
        panic();

    iinic_buffer = buf;
    iinic_buffer_ptr = buf;
    iinic_buffer_end = buf + len;

    if(iinic_buffer_end <= iinic_buffer)
        panic();
}

void iinic_rx()
{
    radio_irq_disable();

    if(IINIC_TX_ACTIVE & state)
        panic();
    if(IINIC_RX_IDLE & state)
        return;

    iinic_buffer_ptr = iinic_buffer;
    iinic_rx_rssi = 0;
    state = IINIC_RX_IDLE;

    radio_mode_rx();
    radio_irq_enable();
}

void iinic_tx()
{
    radio_irq_disable();

    if(IINIC_TX_ACTIVE & state)
        panic();

    iinic_buffer_ptr = iinic_buffer;
    state = IINIC_TX_ACTIVE;
    tx_framing = 0;

    radio_mode_tx();
    radio_irq_enable();
}

void iinic_idle()
{
    radio_irq_disable();

    state = 0;
    iinic_buffer_ptr = iinic_buffer;

    radio_mode_idle();
    radio_irq_enable();
}

uint8_t iinic_instant_poll(uint8_t mask) {
    wdt_reset();
    return mask & must_read(state);
}
uint8_t iinic_infinite_poll(uint8_t mask)
{
    uint8_t v;
    while(!(v = (mask & must_read(state))))
        wdt_reset();
    return v;
}
uint8_t iinic_timed_poll(uint8_t mask, const iinic_timing *deadline)
{
    uint8_t v;
    while(!(v = (mask & must_read(state))) && iinic_now_cmp(deadline) < 0)
        wdt_reset();
    return v;
}

static void do_tx()
{
    if(0 == tx_framing) {
        radio_tx(RADIO_SYNC_BYTE1);
        tx_framing = 1;
        return;
    }
    if(1 == tx_framing) {
        radio_tx(RADIO_SYNC_BYTE2);
        tx_framing = 2;
        return;
    }

    uint8_t *p = iinic_buffer_ptr;
    if(p < iinic_buffer_end) {
        radio_tx(*p++);
        iinic_buffer_ptr = p;
        return;
    }

    if(2 == tx_framing) {
        radio_tx((p[-1] & 0x01) ? 0x55 : 0xAA);
        tx_framing = 3;
        return;
    }

    radio_mode_idle();
    state = IINIC_TX_COMPLETE;
}

static void do_rx()
{
    radio_begin();

    uint8_t status_high = radio_io(0);
    if(!(status_high & _BV(RADIO_FFIT))) {
        radio_end();
        panic();
    }
    uint8_t status_low = radio_io(0);
    uint8_t byte = radio_io(0);

    radio_end();

    bool dqd = 0 != (status_low & _BV(RADIO_DQD));

    if(IINIC_RX_IDLE & state)
    {
        if(!dqd) {
            radio_disable_rxfifo();
            radio_enable_rxfifo();
            return;
        }

        state = IINIC_RX_ACTIVE;
        iinic_get_now(&iinic_rx_timing); /* measured delay since interrupt start: 23.84us
                                            measured execution time : 2.64us */
        iinic_timing_add_32(&iinic_rx_timing, rx_timing_offset);
    }
    else
    {
        if(!dqd)
        {
            state = IINIC_RX_COMPLETE;
            radio_mode_idle();
            return;
        }

        uint16_t rssi = ADC;
        if(rssi > iinic_rx_rssi)
            iinic_rx_rssi = rssi;
    }

    uint8_t *p = iinic_buffer_ptr;
    if(p < iinic_buffer_end) {
        *p++ = byte;
        iinic_buffer_ptr = p;
    } else {
        state = IINIC_RX_COMPLETE | IINIC_RX_OVERFLOW;
        radio_mode_idle();
    }
}

ISR(INT0_vect)
{
    radio_irq_disable();
    sei();

    if(IINIC_TX_ACTIVE & state)
        do_tx();
    else if((IINIC_RX_IDLE | IINIC_RX_ACTIVE) & state)
        do_rx();
    else
        panic();

    cli();
    radio_irq_enable();
}

/* ------------------------------------------------------------------------- */

static void dumb_wait(uint8_t n) /* 1 wait round is ~ 10 ms */
{
    uint16_t i;
    asm volatile(
    "1: wdr\n\t"
       "ldi %A0, 0x00\n\t"
       "ldi %B0, 0x70\n\t"
    "2: adiw %0, 1\n\t" /* 2 cycles */
       "brne 2b\n\t" /* 2 cycles */
       "dec %1\n\t" /* 1 cycle */
       "brne 1b\n\t" /* 2 cycles */
       : "=w" (i), "+r" (n)
    );
}

static void panic()
{
    cli();

    iinic_led_off(IINIC_LED_GREEN);
    iinic_led_on(IINIC_LED_RED);
    for(;;) {
        dumb_wait(20);
        iinic_led_toggle(IINIC_LED_RED);
    }
}
void __iinic_panic() __attribute__ (( noreturn, alias("panic") ));

static uint16_t read_mac()
{
    EEARH = EEARL = 0;
    EECR |= _BV(EERE);
    uint8_t low = EEDR;

    EEARL = 1;
    EECR |= _BV(EERE);
    uint8_t high = EEDR;

    return low | high << 8;
}

static FILE iinic_stdout;
static int usart_tx(char c, FILE *f) {
    (void) f;
    do wdt_reset(); while(!(UCSRA & _BV(UDRE)));
    UDR = c;
    return 0;
}
void iinic_usart_is_debug()
{
    /* usart: 230400, 8n1, no interrupts */
    UBRRH = 0;
    UBRRL = 3;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(RXEN) | _BV(TXEN);

    fdev_setup_stream(&iinic_stdout, usart_tx, NULL, _FDEV_SETUP_WRITE);
    stdout = &iinic_stdout;
}

void iinic_main(void);

int main(void)
{
    /* read the reset reason & clear it */
    uint8_t mcucsr = MCUCSR;
    MCUCSR &= 0xE0;

    /* disable analog comparator */
    ACSR |= _BV(ACD);

    PORTA=0b00000000; DDRA=0b00000000; /* port A: analog inputs + arssi */
    PORTB=0b11111110; DDRB=0b10110000; /* port B: sck,miso,mosi,ss, data,vdi,!radio_rst, button */
    PORTC=0b11111111; DDRC=0b11000000; /* port C: led1,led2, jtag, i2c */
    PORTD=0b11101111; DDRD=0b11110010; /* port D: nc,nc, !usb_rst,!cts, ffit,!radio_irq, txd,rxd */

    /* wait .5s to show the user that we're resetting stuff */
    dumb_wait(50);

    /* spi: no interrupts, master mode, F_CPU/8 */
    SPSR = _BV(SPI2X);
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);

    /* timer 1: normal mode, F_CPU/8, interrupt on overflow */
    TCCR1B = _BV(CS11);
    TIMSK = _BV(TOIE1);

    /* adc: internal 2.56V vref, free-running, 115.2kHz clock */
    ADMUX = _BV(REFS1) | _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

    /* int0: low-level-triggered, not enabled yet */
    MCUCR = 0;

    /* 1sec watchdog */
    WDTCR = _BV(WDE) | _BV(WDP2) | _BV(WDP1);

    /* if we were reset by watchdog, tell everyone */
    if(mcucsr & _BV(WDRF))
        panic();

    /* initialize the radio chip */
    radio_reset();
    radio_set_data_filter();
    iinic_set_frequency(DEFAULT_FREQ);
    iinic_set_bitrate(DEFAULT_BITRATE);
    iinic_set_rx_knobs(DEFAULT_RX_KNOBS);
    iinic_set_tx_knobs(DEFAULT_TX_KNOBS);
    iinic_idle();

    /* read mac from eeprom */
    iinic_mac = read_mac();

    /* show everyone we're up */
    iinic_led_on(IINIC_LED_GREEN);
    sei();

    /* run user code */
    iinic_main();

    /* this should never happen */
    panic();
}
