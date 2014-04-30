#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>

static inline void nop() {
    asm volatile("nop\n\t" ::: "memory");
}

static void reset() __attribute__((noreturn));
static void panic() __attribute__((noreturn));

struct timing {
    uint16_t low;
    uint32_t high;
};

/* ------------------------------------------------------------------------- */

#define must_read(x) (*(const volatile typeof(x) *)&x)
#define must_write(x) (*(volatile typeof(x) *)&x)

static volatile uint8_t data_buf[768];
static volatile uint8_t * data_wr; /* owned by USART_RXC_vect */
static volatile uint8_t * data_rd; /* owned by main */

static volatile uint8_t cmd_buf[64];
static volatile uint8_t * cmd_rd; /* owned by main */
static volatile uint8_t * cmd_wr; /* owned by USART_RXC_vect */
static volatile uint8_t * cmd_rdcap;  /* written by USART_RXC_vect, read by main */

static bool data_buf_empty(volatile uint8_t *cap) {
    return data_rd == cap;
}
static uint8_t data_buf_get() {
    uint8_t byte = *data_rd;
    if(++data_rd == data_buf + sizeof(data_buf))
        data_rd = data_buf;
    return byte;
}
static void data_buf_put(uint8_t byte) {
    volatile uint8_t *wr = must_read(data_wr);
    *wr = byte;
    if(++wr == data_buf + sizeof(data_buf))
        wr = data_buf;
    must_write(data_wr) = wr;
}


static void cmd_buf_put(uint8_t byte) {
    *cmd_wr = byte;
    if(++cmd_wr == cmd_buf + sizeof(cmd_buf))
        cmd_wr = cmd_buf;
}
static void cmd_buf_commit() {
    must_write(cmd_rdcap) = cmd_wr;
}
static bool cmd_buf_empty() {
    return cmd_rd == must_read(cmd_rdcap);
}
static uint8_t cmd_buf_gettoken() {
    return *cmd_rd;
}
static void cmd_buf_eat(void *_out, uint8_t n)
{
    uint8_t *out = (uint8_t *) _out;

    for(;;)
    {
        if(++cmd_rd == cmd_buf + sizeof(cmd_buf))
            cmd_rd = cmd_buf;

        if(0 == n)
            break;
        *out++ = *cmd_rd;
    }
}

/* ------------------------------------------------------------------------- */

enum {
    ESCAPE_BYTE = 0x5a,
    UNESCAPE_TOKEN = 0xa5,
    RESET_RQ_TOKEN = 0x01,
    RESET_ACK_TOKEN = 0x5a,
    RX_KNOBS_TOKEN = 0x02,
    POWER_TAG_TOKEN = 0x03,
    BITRATE_TAG_TOKEN = 0x04,
    TIMING_TAG_TOKEN = 0x05,
    PING_TOKEN = 0x06,
    TX_TOKEN = 0x07,
};

struct rx_knobs_token {
    uint16_t frequency;
    uint8_t deviation;
    uint8_t rx_knobs;
    /* bits [2:0] = rssi threshold
     * bits [4:3] = lna gain,
     * bits [7:5] = bandwidth
     */
};
struct reset_ack_token {
    uint32_t uniq_id;
    struct rx_knobs_token rx_knobs;
    uint8_t power;
    uint8_t bitrate;
    struct timing timing;
};
struct power_tag_token {
    uint8_t power;
};
struct bitrate_tag_token {
    uint8_t bitrate;
};
struct timing_tag_token {
    struct timing timing;
};
struct ping_token {
    uint32_t seq;
};
struct tx_token {
    volatile uint8_t *ptr;
};

/* ------------------------------------------------------------------------- */

/* written by TIMER1_OVF_vect,
 * read by main and INT0_vect,
 * but only via asm code */
static volatile uint32_t timing_high; 

/* written by INT0_vect,
 * read by main */
static volatile struct radio_irq {
    struct timing timing;
    uint8_t flag;
    uint8_t rssi;
} radio_irq;

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

ISR(INT0_vect, ISR_NAKED)
{
    asm volatile(
        "push r24\n\t"
        "push r25\n\t"
        
        "in   r24, 0x2c\n\t" // TCNT1L
        "in   r25, 0x38\n\t" // TIFR
        "sbrc r25, 2\n\t" // TOV1
        "in   r24, 0x2c\n\t" // TCNT1L
        "sts  radio_irq+0, r24\n\t"
        "in   r24, 0x2d\n\t" // TCNT1H
        "sts  radio_irq+1, r24\n\t"

        "lds  r24, timing_high+0\n\t"
        "sts  radio_irq+2, r24\n\t"
        "lds  r24, timing_high+1\n\t"
        "sts  radio_irq+3, r24\n\t"
        "lds  r24, timing_high+2\n\t"
        "sts  radio_irq+4, r24\n\t"
        "lds  r24, timing_high+3\n\t"
        "sts  radio_irq+5, r24\n\t"

        "ldi  r24, 1\n\t"
        "sbrc r25, 2\n\t" // TOV1
        "ldi  r24, 2\n\t"
        "sts  radio_irq+6, r24\n\t"

        /* TODO: read rssi */

        "in   r24, __SREG__\n\t"
        "in   r25, 0x3b\n\t" // GICR
        "cbr  r25, 6\n\t" // INT0
        "out  0x3b, r25\n\t" // GICR
        "out  __SREG__, r24\n\t"
        
        "pop  r25\n\t"
        "pop  r24\n\t"
        "reti"
    );
}

static inline struct timing get_now() __attribute__((always_inline));
static inline struct timing get_now()
{
    struct timing ret;
    uint8_t tmp;
    asm volatile(
        "cli\n\t"
        "in   %A0, 0x2c\n\t" // TCNT1L
        "in   %2, 0x38\n\t" // TIFR
        "sbrc %2, 2\n\t" // TOV1
        "in   %A0, 0x2c\n\t" // TCNT1L
        "in   %B0, 0x2d\n\t" // TCNT1H
        "lds  %A1, timing_high+0\n\t"
        "lds  %B1, timing_high+1\n\t"
        "lds  %C1, timing_high+2\n\t"
        "lds  %D1, timing_high+3\n\t"
        "sei\n\t"

        "andi %2, 0x04\n\t" // TOV1
        "breq 1f\n\t"
        "subi %A1, 0xFF\n\t"
        "sbci %B1, 0xFF\n\t"
        "sbci %C1, 0xFF\n\t"
        "sbci %D1, 0xFF\n\t"
    "1:"
        : "=&r" (ret.low),
          "=&a" (ret.high),
          "=&r" (tmp)
    );
    return ret;
}

static bool timing_geq(const struct timing *a, const struct timing *b)
{
    bool ret;
    asm volatile(
        "cp %A1, %A3\n\t"
        "cpc %B1, %B3\n\t"
        "cpc %A2, %A4\n\t"
        "cpc %B2, %B4\n\t"
        "cpc %C2, %C4\n\t"
        "cpc %D2, %D4\n\t"

        "ldi %0, 0\n\t"
        "brlt 1f\n\t"
        "ldi %0, 1\n\t"
    "1:\n\t"
        : "=&a" (ret)
        : "r" (a->low), "r" (a->high),
          "r" (b->low), "r" (b->high)
    );
    return ret;
}

/* ------------------------------------------------------------------------- */

static volatile uint8_t usart_buf[64];
static volatile uint8_t * usart_rd; /* owned by USART_UDRE_vect */
static volatile uint8_t * usart_wr; /* written by main, read by USART_UDRE_vect */

static bool usart_buf_empty() {
    return usart_rd == must_read(usart_wr);
}
static uint8_t usart_buf_get() {
    uint8_t byte = *usart_rd++;
    if(usart_rd == usart_buf + sizeof(usart_buf))
        usart_rd = usart_buf;
    return byte;
}
static void usart_buf_put(uint8_t byte) {
    volatile uint8_t *wr = must_read(usart_wr);
    *wr++ = byte;
    if(wr == usart_buf + sizeof(usart_buf))
        wr = usart_buf;
    must_write(usart_wr) = wr;
}
static void usart_buf_put_many(const uint8_t *buf, uint8_t n) {
    for(uint8_t i=0; i<n; i++)
        usart_buf_put(buf[i]);
}
static void usart_transmit() {
    UCSRB |= _BV(UDRIE);
}

static const PROGMEM uint8_t usart_token_lengths[] = {
    [RX_KNOBS_TOKEN] = sizeof(struct rx_knobs_token),
    [POWER_TAG_TOKEN] = sizeof(struct power_tag_token),
    [BITRATE_TAG_TOKEN] = sizeof(struct bitrate_tag_token),
    [TIMING_TAG_TOKEN] = sizeof(struct timing_tag_token),
    [PING_TOKEN] = sizeof(struct ping_token),
};

enum {
    USART_RX_IDLE = 0, /* == not parsing anything right now */
    USART_RX_ESCAPE = 0xFF /* == got ESCAPE_BYTE, waiting for token id */
    /* > 0: this many bytes remain for the current token */
};

static void usart_rx(uint8_t data)
{
    static uint8_t state = USART_RX_IDLE;

    if(state == USART_RX_IDLE) {
        if(data != ESCAPE_BYTE)
            data_buf_put(data);
        else
            state = USART_RX_ESCAPE;
        return;
    }
    
    if(state == USART_RX_ESCAPE)
    {
        if(data == UNESCAPE_TOKEN) {
            data_buf_put(ESCAPE_BYTE);
            state = 0;
            return;
        }
        if(data == RESET_RQ_TOKEN) {
            reset();
            return;
        }
        if(data == TX_TOKEN) {
            uint16_t _data_wr = (uint16_t) data_wr;
            cmd_buf_put(data);
            cmd_buf_put(_data_wr & 0xFF);
            cmd_buf_put(_data_wr >> 8);
            cmd_buf_commit();
            state = 0;
            return;
        }

        if(data >= RX_KNOBS_TOKEN && data <= PING_TOKEN) {
            cmd_buf_put(data);
            state = pgm_read_byte(&usart_token_lengths[data]);
            return;
        }

        panic();
        return;
    }

    cmd_buf_put(data);
    if(0 == --state)
        cmd_buf_commit();
}

ISR(USART_RXC_vect)
{
    UCSRB &=~ _BV(RXCIE);
    sei();
    usart_rx(UDR);
    cli();
    UCSRB |= _BV(RXCIE);
}
ISR(USART_TXC_vect)
{
    if(!usart_buf_empty())
        UDR = usart_buf_get();
    else
        UCSRB &=~ _BV(UDRIE);
}

/* ------------------------------------------------------------------------- */

/* radio status: high byte */
#define RADIO_RGIT 7 /* TX register is ready to receive the next byte */
#define RADIO_FFIT 7 /* The number of data bits in the RX FIFO has reached the pre-programmed limit */
#define RADIO_POR  6 /* Power-on reset */
#define RADIO_RGUR 5 /* TX register underrun, register overwrite */
#define RADIO_FFOV 5 /* RX FIFO overflow */
#define RADIO_WKUP 4 /* Wake-up timer overflow */
#define RADIO_EXT  3 /* Logic level on interrupt pin changed to low */
#define RADIO_LBD  2 /* Low battery detected; the power supply voltage is beloe the pre-programmed limit */
#define RADIO_FFEM 1 /* FIFO is empty */
#define RADIO_ATS  0 /* Antenna tuning circuit detected string enough RF signal */
#define RADIO_RSSI 0 /* The strenght of the incomming signal is above the pre-programmed limit */

/* radio status: low byte */
#define RADIO_DQD  7 /* Data quality detector output */
#define RADIO_CRL  6 /* Clock recovery locked */
#define RADIO_ATGL 5 /* AFC cycle */

#define RADIO_SYNC_WORD 0xD4

#define RADIO_DEFAULT_FREQUENCY 0x680 /* 868.32 MHz */
#define RADIO_DEFAULT_BITRATE   5 /* ~9600 bps */
#define RADIO_DEFAULT_POWER     0 /* max */
#define RADIO_DEFAULT_DEVIATION 9 /* 150 kHz */

/* bandwidth = 200 kHz, lna gain = 0dB (max), rssi threshold = -103dB (min) */
#define RADIO_DEFAULT_RX_KNOBS 0x80

static uint8_t radio_power;
static uint8_t radio_deviation;
static uint16_t radio_byte_timing;

static void radio_begin() {
    /* set pin low */
    /* wait > 10ns */
    nop();
    nop();
}
static void radio_end() {
    /* set pin high */
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
    wdt_reset();
    radio_begin();
    radio_io(v >> 8);
    radio_io(v & 0xff);
    radio_end();
}

static void radio_mode_rx() {
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
}
static void radio_mode_tx() {
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

static void radio_set_frequency(uint16_t frequency) {
    /* real freq = 20 * (43 + frequency / 4000) MHz */
    radio_write(0xA000 | frequency);
}
static void radio_set_bitrate(uint8_t bitrate)
{
    /* real bitrate in bps =
     *   msb clear => 10000000 / 29 / (bitrate+1)
     *   msb set   => 10000000 / 29 / (bitrate+1) / 8
     * number of timer 1 ticks it takes to send one byte =
     *   (8 / bitrate) / (16 / fcpu)
     * plugging one into the other gives
     *   byte_timing = 
     *     msb clear => (8 * fcpu * 29) / (16 * 1e+7) * (x+1)
     *     msb set   => (8 * fcpu * 29 * 8) / (16 * 1e+7) * (x+1)
     *  which is
     *     msb clear =>  21.38112 * (x+1)
     *     msb set   => 171.04896 * (x+1)
     */

    uint16_t byte_timing =
        bitrate & 0x80
            ? 171 * ((bitrate & 0x7f) + 1)
            : 21 * (bitrate+1);

    radio_write(0xC600 | bitrate);
    radio_byte_timing = byte_timing;
}
static void radio_set_rx_knobs(uint8_t rx_knobs)
{
    /* 0x9000 = reciever control command
     * p16  = 1 (pin 16 is vdi output)
     * d1:0 = 01 (vdi in medium mode)
     * rx_knobs [2:0] = rssi threshold
     * rx_knobs [4:3] = lna gain
     * rx_knobs [7:5] = bandwidth
     */
    radio_write(0x9500 | rx_knobs);
}
static void radio_set_power_and_deviation(uint8_t power, uint8_t deviation) {
    radio_power = power;
    radio_deviation = deviation;
    radio_write(0x9800 | deviation << 4 | power);
}
static void radio_set_power(uint8_t power) {
    radio_set_power_and_deviation(power, radio_deviation);
}
static void radio_set_deviation(uint8_t deviation) {
    radio_set_power_and_deviation(radio_power, deviation);
}
static void radio_init()
{
    radio_set_frequency(RADIO_DEFAULT_FREQUENCY);
    radio_set_bitrate(RADIO_DEFAULT_BITRATE);
    radio_set_power_and_deviation(RADIO_DEFAULT_POWER, RADIO_DEFAULT_DEVIATION);
    radio_set_rx_knobs(RADIO_DEFAULT_RX_KNOBS);

    /* 0xC200 = data filter command
     * al = 1 (clock recovery in auto mode)
     * ml = 0 (manual clock recovery setting; meaningless)
     * s = 0 (digital data filter)
     * f2:0 = 100 (dqd threshold; default)
     */
    radio_write(0xC2AC);
    
    /* 0xCA00 = fifo and reset mode command
     * f3:0 = 1000 (fifo interrupt threshold; 8 bits = 1 byte = default)
     * sp = 1 (one-byte synchronization pattern)
     * al = 0 (fifo fill start condition: on synchron pattern reception)
     * ff = 1 (fifo fill will be enabled after synchron pattern reception)
     * dr = 0 (sentensive reset mode)
     */
    radio_write(0xCA8A);

    radio_mode_rx();
}

static inline void radio_irq_disable() {
    GICR &= ~_BV(INT0);
}
static inline void radio_irq_enable() {
    if(!radio_irq.flag)
        GICR |= _BV(INT0);
}
static inline void radio_irq_wait() {
    while(PIND & _BV(2))
        wdt_reset();
}

static bool radio_irq_poll(struct radio_irq *irq)
{
    uint8_t flag = radio_irq.flag;
    if(!flag)
        return false;

    irq->timing.low = radio_irq.timing.low;
    irq->timing.high = radio_irq.timing.high;
    if(2 == flag)
        irq->timing.high += 1;
    irq->rssi = radio_irq.rssi;

    radio_irq.flag = 0;
    return true;
}

/* ------------------------------------------------------------------------- */

static struct timing timing_predict;

static bool sync_timing(const struct timing *timing)
{
    /* this computes
     *   p += radio_byte_timing
     *   t = abs(t - p)
     */
    asm volatile(
        "add %A0, %A2\n\t"
        "adc %B0, %B2\n\t"
        "adc %A1, __zero_reg__\n\t"
        "adc %B1, __zero_reg__\n\t"
        "adc %C1, __zero_reg__\n\t"
        "adc %D1, __zero_reg__\n\t"
        : "+&r" (timing_predict.low),
          "+&r" (timing_predict.high)
        : "r" (radio_byte_timing)
    );
    
    struct timing p = { .high = timing_predict.high, .low = timing_predict.low };

    asm volatile(
        "sub %A0, %A2\n\t"
        "sbc %B0, %B2\n\t"
        "sbc %A1, %A3\n\t"
        "sbc %B1, %B3\n\t"
        "sbc %C1, %C3\n\t"
        "sbc %D1, %D3\n\t"

        "brge 1f\n\t"
        "com %D1\n\t"
        "com %C1\n\t"
        "com %B1\n\t"
        "com %A1\n\t"
        "com %B0\n\t"
        "neg %A0\n\t"
        "sbci %B0, 0xFF\n\t"
        "sbci %A1, 0xFF\n\t"
        "sbci %B1, 0xFF\n\t"
        "sbci %C1, 0xFF\n\t"
        "sbci %D1, 0xFF\n\t"
    "1:"
        : "+&a" (p.low),
          "+&a" (p.high)
        : "r" (timing->low),
          "r" (timing->high)
    );

    if(p.high == 0 && p.low <= radio_byte_timing / 2)
        return false;

    timing_predict.low = timing->low;
    timing_predict.high = timing->high;

    return true;
}

static bool op_rx_one()
{
    wdt_reset();

    struct radio_irq irq;
    if(!radio_irq_poll(&irq))
        return false;

    radio_begin();
    uint8_t status_high = radio_io(0);
    uint8_t byte = 0;
    if(status_high & _BV(RADIO_FFIT)) {
        radio_io(0);
        byte = radio_io(0);
    }
    radio_end();

    radio_irq_enable();

    if(!(status_high & _BV(RADIO_FFIT)))
        return false;

    if(sync_timing(&irq.timing))
    {
        usart_buf_put(ESCAPE_BYTE);
        usart_buf_put(TIMING_TAG_TOKEN);
        usart_buf_put(irq.timing.low);
        usart_buf_put(irq.timing.low >> 8);
        usart_buf_put(irq.timing.high);
        usart_buf_put(irq.timing.high >> 8);
        usart_buf_put(irq.timing.high >> 16);
        usart_buf_put(irq.timing.high >> 24);
        
        usart_buf_put(ESCAPE_BYTE);
        usart_buf_put(POWER_TAG_TOKEN);
        usart_buf_put(irq.rssi);
    }

    if(byte != ESCAPE_BYTE)
        usart_buf_put(byte);
    else {
        usart_buf_put(ESCAPE_BYTE);
        usart_buf_put(UNESCAPE_TOKEN);
    }
    
    usart_transmit();

    return true;
}

static void op_rx() {
    while(op_rx_one());
}

static void op_tx(volatile uint8_t *end)
{
    if(data_buf_empty(end))
        return;

    radio_irq_disable();

    
    radio_mode_tx();
    /* radio starts up, first 0xAA is being sent */
    radio_irq_wait();

    /* first 0xAA was sent, second 0xAA is being sent;
     * queue up the sync word 
     * 0xB8 == tx register write command */
    radio_write(0xB800 | RADIO_SYNC_WORD);

    while(!data_buf_empty(end))
    {
        uint8_t byte = data_buf_get();
        radio_irq_wait();
        radio_write(0xB800 | byte);
    }

    /* the penultimate byte is being sent,
     * the ultimate byte is queued */

    radio_irq_wait();
    
    /* the ultimate byte is being sent,
     * write some dummy byte to silence the interrupt */
    radio_write(0xB800 | 0x00);

    radio_irq_wait();
    radio_mode_rx();

    radio_irq_enable();
}

static void op_timing(const struct timing *tptr)
{
    struct timing t = {
        .low = tptr->low,
        .high = tptr->high
    };

    for(;;)
    {
        struct timing now = get_now();
        if(timing_geq(&now, &t))
            break;
        op_rx();
    }
}

static void op_ping(uint32_t seq)
{
    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(PING_TOKEN);
    usart_buf_put(seq);
    usart_buf_put(seq >> 8);
    usart_buf_put(seq >> 16);
    usart_buf_put(seq >> 24);
    usart_transmit();
}

/* ------------------------------------------------------------------------- */

static void panic() {
    cli();
    for(;;);
}

static void reset() {
    cli();
    for(;;);
}

void reset_ack()
{
    struct reset_ack_token token = {
        .uniq_id = eeprom_read_dword(0),
        .rx_knobs = {
            .frequency = RADIO_DEFAULT_FREQUENCY,
            .deviation = RADIO_DEFAULT_DEVIATION,
            .rx_knobs = RADIO_DEFAULT_RX_KNOBS
        },
        .power = RADIO_DEFAULT_POWER,
        .bitrate = RADIO_DEFAULT_BITRATE,
        .timing = get_now()
    };

    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(RESET_ACK_TOKEN);
    usart_buf_put_many((uint8_t *) &token, sizeof(token));
    usart_transmit();
}

int main(void)
{
    ACSR |= _BV(ACD);

    PORTA=0xFF; DDRA=0xFF;
    PORTB=0xFF; DDRB=0xFF;
    PORTC=0xFF; DDRC=0xFF;
    PORTD=0xFF; DDRD=0xFF;

    radio_init();

    reset_ack();

    for(;;)
    {
        op_rx();

        if(cmd_buf_empty())
            continue;

        switch(cmd_buf_gettoken())
        {
            case RX_KNOBS_TOKEN: {
                struct rx_knobs_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_frequency(token.frequency);
                radio_set_deviation(token.deviation);
                radio_set_rx_knobs(token.rx_knobs);
                break; }
            case POWER_TAG_TOKEN: {
                struct power_tag_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_power(token.power);
                break; }
            case BITRATE_TAG_TOKEN: {
                struct bitrate_tag_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_bitrate(token.bitrate);
                break; }
            case TIMING_TAG_TOKEN: {
                struct timing_tag_token token;
                cmd_buf_eat(&token, sizeof(token));
                op_timing(&token.timing);
                break; }
            case PING_TOKEN: {
                struct ping_token token;
                cmd_buf_eat(&token, sizeof(token));
                op_ping(token.seq);
                break; }
            case TX_TOKEN: {
                struct tx_token token;
                cmd_buf_eat(&token, sizeof(token));
                op_tx(token.ptr);
                break; }
            default:
                panic();
                break;
        }
    }
}

