#ifndef IINIC_H
#define IINIC_H

#include <stddef.h>
#include <stdint.h>
#include <avr/io.h>

enum {
    IINIC_LED_RED =  0x80,
    IINIC_LED_GREEN = 0x40
};

static inline void iinic_led_on(uint8_t led) { PORTC &=~ led; }
static inline void iinic_led_off(uint8_t led) { PORTC |= led; }
static inline void iinic_led_toggle(uint8_t led) { PORTC ^= led; }

typedef struct {
    uint8_t b[6];
} iinic_timing;

void iinic_timing_add(iinic_timing *a, const iinic_timing *b);
void iinic_timing_add_32(iinic_timing *a, int32_t b);
void iinic_timing_sub(iinic_timing *a, const iinic_timing *b);
int8_t iinic_timing_cmp(const iinic_timing *a, const iinic_timing *b);
int8_t iinic_now_cmp(const iinic_timing *ref);
void iinic_get_now(iinic_timing *out);

enum {
    IINIC_RSSI_103 = 0,
    IINIC_RSSI_97  = 1,
    IINIC_RSSI_91  = 2,
    IINIC_RSSI_85  = 3,
    IINIC_RSSI_79  = 4,
    IINIC_RSSI_73  = 5,

    IINIC_GAIN_0   = 0 << 3,
    IINIC_GAIN_6   = 1 << 3,
    IINIC_GAIN_14  = 2 << 3,
    IINIC_GAIN_20  = 3 << 3,

    IINIC_BW_400 = 1 << 5,
    IINIC_BW_340 = 2 << 5,
    IINIC_BW_270 = 3 << 5,
    IINIC_BW_200 = 4 << 5,
    IINIC_BW_134 = 5 << 5,
    IINIC_BW_67  = 6 << 5,

    IINIC_POWER_0   = 0,
    IINIC_POWER_25  = 1,
    IINIC_POWER_50  = 2,
    IINIC_POWER_75  = 3,
    IINIC_POWER_100 = 4,
    IINIC_POWER_125 = 5,
    IINIC_POWER_150 = 6,
    IINIC_POWER_175 = 7,

    IINIC_DEVIATION_15   =  0 << 4,
    IINIC_DEVIATION_30   =  1 << 4,
    IINIC_DEVIATION_45   =  2 << 4,
    IINIC_DEVIATION_60   =  3 << 4,
    IINIC_DEVIATION_75   =  4 << 4,
    IINIC_DEVIATION_90   =  5 << 4,
    IINIC_DEVIATION_105  =  6 << 4,
    IINIC_DEVIATION_120  =  7 << 4,
    IINIC_DEVIATION_135  =  8 << 4,
    IINIC_DEVIATION_150  =  9 << 4,
    IINIC_DEVIATION_165  = 10 << 4,
    IINIC_DEVIATION_180  = 11 << 4,
    IINIC_DEVIATION_195  = 12 << 4,
    IINIC_DEVIATION_210  = 13 << 4,
    IINIC_DEVIATION_225  = 14 << 4,
    IINIC_DEVIATION_240  = 15 << 4,
    IINIC_DEVIATION_INV  =  1 << 8,

    IINIC_BITRATE_600    = 0x80 |  71, //   598.659 bps
    IINIC_BITRATE_1200   = 0x80 |  35, //  1197.318 bps
    IINIC_BITRATE_2400   =        143, //  2394.636 bps
    IINIC_BITRATE_3600   =         95, //  3591.954 bps
    IINIC_BITRATE_4800   =         71, //  4789.272 bps
    IINIC_BITRATE_9600   =         35, //  9578.544 bps
    IINIC_BITRATE_11400  =         29, // 11494.253 bps
    IINIC_BITRATE_19200  =         17, // 19157.088 bps
    IINIC_BITRATE_28800  =         11, // 28735.632 bps
    IINIC_BITRATE_38400  =          8, // 38314.176 bps, too fast
    IINIC_BITRATE_57600  =          5, // 57471.264 bps, too fast
    IINIC_BITRATE_115200 =          2, // 114942.534 bps, too fast
};

/* internal, do not call */
void __iinic_radio_write(uint16_t);

static inline void iinic_set_frequency(uint16_t frequency) {
    /* real freq = 20 * (43 + frequency / 4000) MHz */
    __iinic_radio_write(0xA000 | frequency);
}
static inline void iinic_set_bitrate(uint8_t bitrate)
{
    /* real bitrate in bps =
     *   msb clear => 10000000 / 29 / (bitrate+1)
     *   msb set   => 10000000 / 29 / (bitrate+1) / 8
     */
    __iinic_radio_write(0xC600 | bitrate);
}
static inline void iinic_set_rx_knobs(uint8_t rx_knobs)
{
    /* 0x9000 = reciever control command
     * p16  = 1 (pin 16 is vdi output)
     * d1:0 = 00 (vdi in fast, vdi = dqd)
     * rx_knobs [2:0] = rssi threshold
     * rx_knobs [4:3] = lna gain
     * rx_knobs [7:5] = bandwidth
     */
    __iinic_radio_write(0x9500 | rx_knobs);
}

static inline void iinic_set_tx_knobs(uint8_t tx_knobs) {
    /* 0x9800 = tx configuration control command
     * tx_knobs [8] = invert deviation polarization
     * tx_knobs [7:4] = deviation (15kHz * (1+deviation))
     * tx_knobs [3:0] = output power
     */
    __iinic_radio_write(0x9800 | tx_knobs);
}

enum {
    IINIC_RX_IDLE = _BV(0),
    IINIC_RX_ACTIVE = _BV(1),
    IINIC_RX_COMPLETE = _BV(2),
    IINIC_RX_OVERFLOW = _BV(3),

    IINIC_TX_ACTIVE = _BV(4),
    IINIC_TX_COMPLETE = _BV(5),
};

extern uint16_t iinic_mac;

extern uint8_t *iinic_buffer,
               *iinic_buffer_ptr,
               *iinic_buffer_end;

extern uint16_t iinic_rx_rssi;
extern iinic_timing iinic_rx_timing;

void iinic_set_buffer(uint8_t *buf, uint16_t len);

void iinic_rx();
void iinic_tx();
void iinic_idle();

uint8_t iinic_infinite_poll(uint8_t mask);
uint8_t iinic_instant_poll(uint8_t mask);
uint8_t iinic_timed_poll(uint8_t mask, const iinic_timing *deadline);

void iinic_usart_is_debug();

#endif

