#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

static inline void nop() __attribute__((always_inline));
static inline void nop() {
    asm volatile("nop\n\t" ::: "memory");
}

static void reset() __attribute__((noreturn));
static inline void panic_inline() __attribute__((noreturn)) __attribute__((always_inline));
static void panic() __attribute__((noreturn));

struct timing {
    uint8_t b[6];
};

/* ------------------------------------------------------------------------- */

#define must_read(x) (*(const volatile typeof(x) *)&x)
#define must_write(x) (*(volatile typeof(x) *)&x)

static volatile uint8_t data_buf[1536];
static volatile uint8_t * data_wr = data_buf; /* owned by USART_RXC_vect */
static volatile uint8_t * data_rd = data_buf; /* owned by main */

static volatile uint8_t cmd_buf[128];
static uint8_t cmd_rd; /* owned by main */
static uint8_t cmd_wr; /* owned by USART_RXC_vect */
static uint8_t cmd_rdcap;  /* written by USART_RXC_vect, read by main */

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


static inline void cmd_buf_put(uint8_t byte) {
    cmd_buf[cmd_wr] = byte;
    cmd_wr = (cmd_wr + 1) & (sizeof(cmd_buf) - 1);
}
static void cmd_buf_commit() {
    must_write(cmd_rdcap) = cmd_wr;
}
static bool cmd_buf_empty() {
    return cmd_rd == must_read(cmd_rdcap);
}
static uint8_t cmd_buf_gettoken() {
    return cmd_buf[cmd_rd];
}
static void cmd_buf_eat(void *_out, uint8_t n)
{
    uint8_t *out = (uint8_t *) _out,
            *end = out + n;

    for(;;)
    {
        cmd_rd = (cmd_rd + 1) & (sizeof(cmd_buf) - 1);
        if(out == end)
            break;
        *out++ = cmd_buf[cmd_rd];
    }
}

/* ------------------------------------------------------------------------- */

enum {
    ESCAPE_BYTE = 0x5a,
    UNESCAPE_TOKEN = 0xa5,
    RESET_RQ_TOKEN = 0x01,
    RESET_ACK_TOKEN = 0x5a,
    SET_RX_KNOBS_TOKEN = 0x02,
    SET_POWER_TOKEN = 0x03,
    SET_BITRATE_TOKEN = 0x04,
    TIMING_TOKEN = 0x05,
    PING_TOKEN = 0x06,
    TX_TOKEN = 0x07,
    FRAME_START_TOKEN = 0x08,
    FRAME_END_TOKEN = 0x09,
};

struct reset_ack_token {
    uint8_t version_high;
    uint8_t version_low;
    uint16_t uniq_id;
};
struct set_rx_knobs_token {
    uint16_t frequency;
    uint8_t deviation;
    uint8_t rx_knobs;
    /* bits [2:0] = rssi threshold
     * bits [4:3] = lna gain
     * bits [7:5] = bandwidth
     */
};
struct set_power_token {
    uint8_t power;
};
struct set_bitrate_token {
    uint8_t bitrate;
};
struct timing_token {
    struct timing timing;
};
struct ping_token {
    uint8_t seq;
};
struct tx_token {
    volatile uint8_t *ptr;
};
struct frame_start_token {
    struct timing timing;
    uint8_t rssi;
};
struct frame_end_token {
};

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

static inline struct timing get_now() __attribute__((always_inline));
static inline struct timing get_now()
{
    struct timing ret;
    uint8_t tmp;
    asm volatile(
        "cli\n\t"
        "in   %0, 0x2c\n\t" // TCNT1L
        "in   %6, 0x38\n\t" // TIFR
        "sbrc %6, 2\n\t" // TOV1
        "in   %0, 0x2c\n\t" // TCNT1L
        "in   %1, 0x2d\n\t" // TCNT1H
        "lds  %2, timing_high+0\n\t"
        "lds  %3, timing_high+1\n\t"
        "lds  %4, timing_high+2\n\t"
        "lds  %5, timing_high+3\n\t"
        "sei\n\t"

        "andi %6, 0x04\n\t" // TOV1
        "breq 1f\n\t"
        "subi %2, 0xFF\n\t"
        "sbci %3, 0xFF\n\t"
        "sbci %4, 0xFF\n\t"
        "sbci %5, 0xFF\n\t"
    "1:"
        : "=&r" (ret.b[0]),
          "=&r" (ret.b[1]),
          "=&a" (ret.b[2]),
          "=&a" (ret.b[3]),
          "=&a" (ret.b[4]),
          "=&a" (ret.b[5]),
          "=&r" (tmp)
    );
    return ret;
}

static bool timing_geq(const struct timing *a, const struct timing *b)
{
    bool ret;
    asm volatile(
        "cp %1, %7\n\t"
        "cpc %2, %8\n\t"
        "cpc %3, %9\n\t"
        "cpc %4, %10\n\t"
        "cpc %5, %11\n\t"
        "cpc %6, %12\n\t"

        "ldi %0, 0\n\t"
        "brlt 1f\n\t"
        "ldi %0, 1\n\t"
    "1:\n\t"
        : "=&a" (ret)
        : "r" (a->b[0]), "r" (a->b[1]), "r" (a->b[2]),
          "r" (a->b[3]), "r" (a->b[4]), "r" (a->b[5]),
          "r" (b->b[0]), "r" (b->b[1]), "r" (b->b[2]),
          "r" (b->b[3]), "r" (b->b[4]), "r" (b->b[5])
    );
    return ret;
}

/* ------------------------------------------------------------------------- */

static volatile uint8_t usart_buf[128];
static uint8_t usart_rd __attribute__((used)); /* owned by USART_UDRE_vect */
static uint8_t usart_rdcap __attribute__((used)); /* written by main, read by USART_UDRE_vect */
static uint8_t usart_wr; /* owned by main */

static inline void usart_buf_put(uint8_t byte) {
    usart_buf[usart_wr] = byte;
    usart_wr = (usart_wr + 1) & (sizeof(usart_buf) - 1);
}
#if 0
static void usart_buf_put_hex(uint8_t byte) {
    uint8_t x;
    x = byte >> 4;
    usart_buf_put(x < 10 ? '0' + x : 'A' + x - 10);
    x = byte & 0x0F;
    usart_buf_put(x < 10 ? '0' + x : 'A' + x - 10);
}
#endif
static inline void usart_transmit() {
    must_write(usart_rdcap) = usart_wr;
    UCSRB |= _BV(UDRIE);
}

static const PROGMEM uint8_t usart_token_lengths[] = {
    [SET_RX_KNOBS_TOKEN] = sizeof(struct set_rx_knobs_token),
    [SET_POWER_TOKEN] = sizeof(struct set_power_token),
    [SET_BITRATE_TOKEN] = sizeof(struct set_bitrate_token),
    [TIMING_TOKEN] = sizeof(struct timing_token),
    [PING_TOKEN] = sizeof(struct ping_token),
};

enum {
    USART_RX_IDLE = 0, /* == not parsing anything right now */
    USART_RX_ESCAPE = 0xFF /* == got ESCAPE_BYTE, waiting for token id */
    /* > 0: this many bytes remain for the current token */
};

ISR(USART_RXC_vect)
{
    static uint8_t state = USART_RX_IDLE;

    uint8_t data = UDR;

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

        if(data >= SET_RX_KNOBS_TOKEN && data <= PING_TOKEN) {
            cmd_buf_put(data);
            state = pgm_read_byte(&usart_token_lengths[data]);
            return;
        }

        panic_inline();
    }

    cmd_buf_put(data);
    if(0 == --state)
        cmd_buf_commit();
}

ISR(USART_UDRE_vect, ISR_NAKED)
{
    asm volatile(
        "push r26\n\t"
        "push r27\n\t"
        "in   r26, __SREG__\n\t"
        "push r26\n\t"

        "lds  r26, usart_rd\n\t"
        "lds  r27, usart_rdcap\n\t"
        "cp   r26, r27\n\t"
        "breq 1f\n\t"

        "eor  r27, r27\n\t"
        "subi r26, lo8(-(usart_buf))\n\t"
        "sbci r27, hi8(-(usart_buf))\n\t"
        "ld   r26, X\n\t"
        "out  0x0c, r26\n\t" // UDR

        "lds  r26, usart_rd\n\t"
        "inc  r26\n\t"
        "andi r26, 0x7F\n\t" // sizeof(usart_buf)-1
        "sts  usart_rd, r26\n\t"
        "jmp  2f\n\t"

   "1:   cbi 0x0a, 5\n\t" // UCSRB, UDRIE

   "2:   pop  r26\n\t"
        "out  __SREG__, r26\n\t"
        "pop  r27\n\t"
        "pop  r26\n\t"
        "reti\n\t"
    );
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

#define RADIO_SYNC_LEN 1
#define RADIO_SYNC_BYTE1 0x2D
#define RADIO_SYNC_BYTE2 0xD4

/* ------- the state -------  */

static uint8_t radio_power = 0; /* radio's power-on-reset default */
static uint8_t radio_deviation = 0; /* radio's power-on-reset default */
static uint16_t radio_byte_timing;

enum {
    RADIO_CFG_BITRATE = _BV(0),
    RADIO_CFG_POWER = _BV(1),
    RADIO_CFG_RX_KNOBS = _BV(2),
    RADIO_CFG_OK = _BV(3)
};
static uint8_t radio_cfg;

enum {
    RADIO_RX_IDLE  = 0x0000,
    RADIO_RX_FIRST = 0x0100,
    RADIO_RX_DATA  = 0x0200,
    RADIO_RX_END   = 0x0400,
    RADIO_RX_LOOP  = 0x0800
};

static uint16_t radio_rx_not_started();
static uint16_t radio_rx_vdi_wait();
static uint16_t radio_rx_data_wait();
static uint16_t radio_rx_on_air();
static uint16_t radio_rx_aborted();

static struct {
    uint16_t (*state)();
    uint16_t deadline;
    uint8_t rounds;
} radio_rx_state;

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

static inline bool radio_get_irq() {
    return !(PIND & _BV(2));
}
static inline bool radio_get_vdi() {
    return !!(PINB & _BV(2));
}
static inline void radio_irq_wait() {
    do
        wdt_reset();
    while(!radio_get_irq());
}

/* ------- settings -------  */

static void radio_mode_rx()
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
}
static void radio_mode_tx()
{
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
     *   (8 / bitrate) / (8 / fcpu)
     * plugging one into the other gives
     *   byte_timing = 
     *     msb clear => (8 * fcpu * 29) / (8 * 1e+7) * (x+1)
     *     msb set   => (8 * fcpu * 29 * 8) / (8 * 1e+7) * (x+1)
     *  which is
     *     msb clear =>  42.76224 * (x+1)
     *     msb set   => 342.09792 * (x+1)
     */

    uint16_t byte_timing =
        bitrate & 0x80
            ? 342 * ((bitrate & 0x7f) + 1)
            : 43 * (bitrate+1);

    radio_write(0xC600 | bitrate);
    radio_byte_timing = byte_timing;
}
static void radio_set_rx_knobs(uint8_t rx_knobs)
{
    /* 0x9000 = reciever control command
     * p16  = 1 (pin 16 is vdi output)
     * d1:0 = 01 (vdi in medium mode, ie. (rssi or dqd) and clock_locked)
     * rx_knobs [2:0] = rssi threshold
     * rx_knobs [4:3] = lna gain
     * rx_knobs [7:5] = bandwidth
     */
    radio_write(0x9500 | rx_knobs);
}
static void radio_enable_rxfifo()
{
    /* 0xCA00 = fifo and reset mode command
     * f3:0 = 1000 (fifo interrupt threshold; 8 bits = 1 byte = default)
     * sp = ? (0 = two-byte / 1 = one-byte synchronization pattern)
     * al = 0 (fifo fill start condition: on synchron pattern reception)
     * ff = 1 (fifo fill will be enabled after synchron pattern reception)
     * dr = 0 (sentensive reset mode)
     */
#if RADIO_SYNC_LEN == 2
    radio_write(0xCA82);
#else
    radio_write(0xCA8A);
#endif
}
static void radio_disable_rxfifo()
{
    /* 0xCA00 = fifo and reset mode command
     * f3:0 = 1000 (fifo interrupt threshold; 8 bits = 1 byte = default)
     * sp = ? (0 = two-byte / 1 = one-byte synchronization pattern)
     * al = 0 (fifo fill start condition: on synchron pattern reception)
     * ff = 0 (fifo fill disabled)
     * dr = 0 (sentensive reset mode)
     */
#if RADIO_SYNC_LEN == 2
    radio_write(0xCA80);
#else
    radio_write(0xCA88);
#endif
}
static void radio_set_data_filter()
{
    /* 0xC200 = data filter command
     * al = 1 (clock recovery in auto mode)
     * ml = 0 (manual clock recovery setting; meaningless)
     * s = 0 (digital data filter)
     * f2:0 = 111 (dqd threshold; max)
     */
    radio_write(0xC2AF);
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

/* ------- start-up sequence -------  */

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
    
    /* read out power-on-reset status */
    radio_irq_wait();
    radio_begin();
    uint8_t status_high = radio_io(0);
    radio_io(0);
    radio_end();

    if(status_high != _BV(RADIO_POR))
        panic();

    radio_rx_state.state = radio_rx_not_started;
}

static bool radio_is_started() {
    return radio_cfg == RADIO_CFG_OK;
}
static void radio_start(uint8_t cfg)
{
    if(radio_is_started())
        return;

    radio_cfg |= cfg;
    if(radio_cfg == RADIO_CFG_OK - 1)
    {
        radio_set_data_filter();
        radio_disable_rxfifo();
        radio_mode_rx();

        radio_cfg = RADIO_CFG_OK;
        radio_rx_state.state = radio_rx_vdi_wait;
    }
}

/* ------- rx state machine -------  */

static void radio_rx_abort()
{
    if(radio_rx_state.state == radio_rx_not_started ||
       radio_rx_state.state == radio_rx_vdi_wait ||
       radio_rx_state.state == radio_rx_aborted)
        return;

    radio_disable_rxfifo();

    radio_rx_state.state =
        radio_rx_state.state == radio_rx_on_air
            ? radio_rx_aborted
            : radio_rx_vdi_wait;
}

static uint8_t radio_do_rx()
{
    radio_begin();
    uint8_t status_high = radio_io(0);
    uint8_t byte = 0;
    if(status_high & _BV(RADIO_FFIT)) {
        radio_io(0);
        byte = radio_io(0);
    }
    radio_end();

    if(!(status_high & _BV(RADIO_FFIT)))
        panic();

    return byte;
}

static uint16_t radio_rx_not_started() {
    return RADIO_RX_IDLE;
}
static uint16_t radio_rx_vdi_wait()
{
    if(!radio_get_vdi())
        return RADIO_RX_IDLE;

    radio_enable_rxfifo();
    radio_rx_state.deadline = TCNT1 + radio_byte_timing;
    radio_rx_state.rounds =
        (2 + RADIO_SYNC_LEN + 1) /* preamble, sync and the data byte we're waiting for  */
        + 1; /* error margin */
    radio_rx_state.state = radio_rx_data_wait;
    return RADIO_RX_IDLE;
}
static uint16_t radio_rx_data_wait()
{
    if(!radio_get_vdi()) {
        radio_rx_abort();
        return RADIO_RX_IDLE;
    }
    if(radio_get_irq()) {
        radio_rx_state.state = radio_rx_on_air;
        return RADIO_RX_FIRST | radio_do_rx();
    }
    if((int16_t)(radio_rx_state.deadline - TCNT1) < 0)
    {
        if(--radio_rx_state.rounds > 0)
            radio_rx_state.deadline += radio_byte_timing;
        else
            radio_rx_abort();
        return RADIO_RX_IDLE;
    }
    return RADIO_RX_IDLE;
}
static uint16_t radio_rx_on_air()
{
    if(!radio_get_vdi()) {
        radio_rx_abort();
        return RADIO_RX_LOOP;
    }
    if(!radio_get_irq())
        return RADIO_RX_IDLE;
    return RADIO_RX_DATA | radio_do_rx();
}
static uint16_t radio_rx_aborted() {
    radio_rx_state.state = radio_rx_vdi_wait;
    return RADIO_RX_END;
}

static inline uint16_t radio_rx() {
    wdt_reset();
    return radio_rx_state.state();
}

/* ------------------------------------------------------------------------- */

static void op_rx_frame_start()
{
    struct timing timing = get_now();
    uint8_t rssi = 42; /* TODO: measure this actually */

    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(FRAME_START_TOKEN);
    usart_buf_put(timing.b[0]);
    usart_buf_put(timing.b[1]);
    usart_buf_put(timing.b[2]);
    usart_buf_put(timing.b[3]);
    usart_buf_put(timing.b[4]);
    usart_buf_put(timing.b[5]);
    usart_buf_put(rssi);
}
static void op_rx_data(uint8_t byte)
{
    if(byte != ESCAPE_BYTE)
        usart_buf_put(byte);
    else {
        usart_buf_put(ESCAPE_BYTE);
        usart_buf_put(UNESCAPE_TOKEN);
    }
}
static void op_rx_frame_end()
{
    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(FRAME_END_TOKEN);
}
static void op_do_rx(uint16_t rx) __attribute__((noinline));
static void op_do_rx(uint16_t rx)
{
    if(rx & RADIO_RX_FIRST)
        op_rx_frame_start();
    if(rx & (RADIO_RX_FIRST | RADIO_RX_DATA))
        op_rx_data(rx & 0xFF);
    if(rx & RADIO_RX_END)
        op_rx_frame_end();
    usart_transmit();
}
static void op_rx()
{
    uint16_t rx;
    while((rx = radio_rx()) != RADIO_RX_IDLE)
        op_do_rx(rx);
}

static void op_tx(volatile uint8_t *end)
{
    if(!radio_is_started())
        panic();

    if(data_buf_empty(end))
        return;

    radio_rx_abort();
    op_rx();

    radio_mode_tx();
    radio_irq_wait();

#if RADIO_SYNC_LEN == 2
    /* first 0xAA was sent, second 0xAA is being sent;
     * queue up the first sync byte
     * 0xB8 == tx register write command */
    radio_write(0xB800 | RADIO_SYNC_BYTE1);
    
    radio_irq_wait();
#endif

    /* second 0xAA was sent, first sync byte is being sent;
     * queue up the second sync byte
     * 0xB8 == tx register write command */
    radio_write(0xB800 | RADIO_SYNC_BYTE2);

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
}

static void op_timing(const struct timing *tptr)
{
    for(;;)
    {
        struct timing now = get_now();
        if(timing_geq(&now, tptr))
            break;
        op_rx();
    }
}

static void op_ping(uint32_t seq)
{
    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(PING_TOKEN);
    usart_buf_put(seq);
    usart_transmit();
}

static uint16_t read_uniq_id()
{
    EEARH = EEARL = 0;
    EECR |= _BV(EERE);
    uint8_t low = EEDR;

    EEARL = 1;
    EECR |= _BV(EERE);
    uint8_t high = EEDR;

    return low | high << 8;
}

static void op_hello()
{
    uint8_t version_high = 1,
            version_low = 0;
    uint16_t uniq_id = read_uniq_id();

    usart_buf_put(ESCAPE_BYTE);
    usart_buf_put(RESET_ACK_TOKEN);
    usart_buf_put(version_high);
    usart_buf_put(version_low);
    usart_buf_put(uniq_id & 0xFF); 
    usart_buf_put(uniq_id >> 8);

    usart_transmit();
}

/* ------------------------------------------------------------------------- */

#define SOFT_RESET_MAGIC   0x18b8fc88db2afc2all
#define SOFT_RESET_CLOBBER 0xffffffffffffffffll
static volatile uint64_t soft_reset_magic __attribute__((section(".noinit")));

static void dumb_wait(uint8_t n) /* 1 wait round is ~ 17.(7) ms */
{
    uint16_t i;
    asm volatile(
       "wdr\n\t"
       "eor %A0, %A0\n\t"
       "eor %B0, %B0\n\t"
    "1: adiw %0, 1\n\t" /* 2 cycles */
       "brne 1b\n\t" /* 2 cycles */
       "wdr\n\t" /* 1 cycle */
       "dec %1\n\t" /* 1 cycle */
       "brne 1b\n\t" /* 2 cycles */
       : "=w" (i), "+r" (n)
    );
}

#define LED_RED 0x80
#define LED_GREEN 0x40
static inline void led_on(uint8_t led) { PORTC &=~ led; }
static inline void led_off(uint8_t led) { PORTC |= led; }
static inline void led_toggle(uint8_t led) { PORTC ^= led; }

static inline void panic_inline()
{
    cli();

    led_off(LED_GREEN);
    led_on(LED_RED);
    for(;;) wdt_reset();
}
static void panic() {
    panic_inline();
}

static void reset()
{
    cli();

    soft_reset_magic = SOFT_RESET_MAGIC;
    for(;;);
}

static void await_reset_cmd()
{
    for(;;) {
        dumb_wait(12);
        led_toggle(LED_GREEN);
    }
}

int main(void)
{
    /* read the reset reason & clear it */
    uint8_t mcucsr = MCUCSR;
    MCUCSR &= 0xE0;

    /* read the soft reset flag & clear it */
    bool soft_reset_flag = (soft_reset_magic == SOFT_RESET_MAGIC);
    soft_reset_magic = SOFT_RESET_CLOBBER;

    /* disable analog comparator */
    ACSR |= _BV(ACD);

    PORTA=0b00000000; DDRA=0b00000000; /* port A: analog inputs + arssi */
    PORTB=0b11111111; DDRB=0b10110001; /* port B: sck,miso,mosi,ss, data,vdi,!radio_rst, nc */
    PORTC=0b11111111; DDRC=0b11000000; /* port C: led1,led2, jtag, i2c */
    PORTD=0b11101111; DDRD=0b11110010; /* port D: nc,nc, !usb_rst,!cts, ffit,!radio_irq, txd,rxd */

    /* wait around .5s to show the user that we're resetting stuff */
    dumb_wait(30);

    /* usart: 230400, 8n1, rx interrupt, tx idle interrupt */
    UBRRH = 0;
    UBRRL = 3;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(RXCIE) | _BV(UDRIE) | _BV(RXEN) | _BV(TXEN);

    /* spi: no interrupts, master mode, F_CPU/8 */
    SPSR = _BV(SPI2X);
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);

    /* timer 1: normal mode, F_CPU/8, interrupt on overflow */
    TCCR1B = 0x02;
    TIMSK = _BV(TOIE1);

    /* 65ms watchdog */
    WDTCR = _BV(WDE) | _BV(WDP1);

    sei();

    if(!(mcucsr & _BV(WDRF)))
        await_reset_cmd();

    if(!soft_reset_flag) {
        for(;;) {
            dumb_wait(15);
            led_toggle(LED_RED);
        }
    }

    led_on(LED_GREEN);

    radio_reset();
    op_hello();

    for(;;)
    {
        op_rx();

        if(cmd_buf_empty())
            continue;

        switch(cmd_buf_gettoken())
        {
            case SET_RX_KNOBS_TOKEN: {
                struct set_rx_knobs_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_frequency(token.frequency);
                radio_set_deviation(token.deviation);
                radio_set_rx_knobs(token.rx_knobs);
                radio_start(RADIO_CFG_RX_KNOBS);
                radio_rx_abort();
                break; }
            case SET_POWER_TOKEN: {
                struct set_power_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_power(token.power);
                radio_start(RADIO_CFG_POWER);
                radio_rx_abort();
                break; }
            case SET_BITRATE_TOKEN: {
                struct set_bitrate_token token;
                cmd_buf_eat(&token, sizeof(token));
                radio_set_bitrate(token.bitrate);
                radio_start(RADIO_CFG_BITRATE);
                radio_rx_abort();
                break; }
            case TIMING_TOKEN: {
                struct timing_token token;
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

