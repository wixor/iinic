#include <util/crc16.h>
#include <avr/interrupt.h>

#include "iinic.h"

#define const_fn __attribute__((const))
#define pure_fn __attribute__((pure))

typedef union {
    iinic_timing t;
    struct {
        uint16_t round_offset;
        int32_t seq;
    };
} round_timing;

enum { ROUND_OFFSET_THRESHOLD = 512 }; /* 2 bytes */
static uint16_t round_offset;
static int32_t seq_delta;

struct packet_hdr {
    uint16_t sender;
    int32_t seq;
    uint8_t length;
    unsigned neighbour_infos : 4;
    unsigned button_dist : 4;
    unsigned mask_id : 2;
    unsigned _unused : 6;
};

struct packet_neighbour_info {
    uint16_t who;
    unsigned mask_id : 2;
    unsigned match : 5;
    unsigned _unused : 5;
    unsigned button_dist : 4;
};

struct node_info {
    uint16_t who;
    int32_t seq_1, seq_2;
    uint16_t rssi;
    bool tier_one;
    bool sent;
    uint8_t match;
    uint8_t mask;
    uint8_t button_dist;
};

enum {
    NODES_COUNT = 16,
    NODE_TIER_1_TTL = 96,
    NODE_TIER_2_TTL = 384,
    PACKET_MAX_LENGTH = 248,
    PACKET_MAX_NODES = 15,
    SCHEDULE_PERIOD = 64,
};

static uint8_t buf1[PACKET_MAX_LENGTH], buf2[PACKET_MAX_LENGTH];
static struct node_info nodes[NODES_COUNT];
static uint8_t nodes_count;
static bool nodes_changed;
static uint8_t until_schedule;
static uint8_t tx_mask, tx_match;

static volatile uint8_t my_button_dist;
static volatile uint8_t my_connectivity; // 0 = not connected, 1 = some tier2-s, 2 = have tier1

/* ------------------------------------------------------------------------- */

static uint16_t pure_fn compute_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xffff;
    while(len) {
        crc = _crc16_update(crc, *buf++);
        len--;
    }
    return crc;
}
static inline bool pure_fn check_crc(const uint8_t *buf, uint16_t len) {
    return 0 == compute_crc(buf, len);
}
static inline void overwrite_crc(uint8_t *buf, uint16_t len) {
    *(uint16_t *)(buf+len-2) = compute_crc(buf, len-2);
}

/* ------------------------------------------------------------------------- */

static uint32_t const_fn get_tx_bitmap(uint8_t mask, uint8_t match)
{
    uint32_t ret = 0;
    for(int8_t i=31; i>=0; i--) {
        ret <<= 1;
        if((i & mask) == match)
            ret |= 1;
    }
    return ret;
}

static uint8_t const_fn mask_id_enc(uint8_t mask)
{
    uint8_t ret = 0;
    while(mask > 3) {
        ret++;
        mask >>= 1;
    }
    return ret;
}

static uint8_t const_fn mask_id_dec(uint8_t mask_id)
{
    uint8_t ret = 4;
    while(mask_id) {
        ret <<= 1;
        mask_id--;
    }
    return ret-1;
}

static inline bool pure_fn should_tx(uint8_t seq) {
    return (seq & tx_mask) == tx_match;
}

static bool pure_fn check_collision()
{
    if(0 == tx_mask)
        return false;
    for(struct node_info *n = nodes; n < nodes + nodes_count; n++)
        if((tx_mask & n->mask & n->match) == (tx_mask & n->mask & tx_match))
            return true;
    return false;
}

static uint8_t const_fn score_tx_bitmap(uint32_t forbidden)
{
    for(uint8_t mask = 3; mask <= 31; mask = (mask<<1) | 1)
    {
        uint8_t missed = 0;
        for(uint8_t match = 0; match <= mask; match++)
            if(forbidden & get_tx_bitmap(mask, match))
                missed++;
        if(missed < mask+1)
            return (mask+1) | missed;
    }
    return 0xff;
}

static void schedule(int32_t seq)
{
    for(struct node_info *n = nodes; n < nodes + nodes_count; )
    {
        if(n->tier_one && n->seq_1 < seq - NODE_TIER_1_TTL) {
            debug("demoting node %04x to tier 2\r\n", n->who);
            n->tier_one = false;
            nodes_changed = true;
        }
        if(n->seq_2 < seq - NODE_TIER_2_TTL) {
            debug("dropping node %04x, too old\r\n", n->who);
            *n = nodes[--nodes_count];
            nodes_changed = true;
        } else
            n++;
    }

    if(nodes_changed) {
        nodes_changed = false;
        if(check_collision()) {
            debug("collision, scheduling very soon!\r\n");
            until_schedule = (until_schedule & 7) | 1;
        }
    }

    if(0 == nodes_count) {
        tx_mask = 0xff;
        tx_match = 0;
        until_schedule = 0;
        return;
    }

    if(--until_schedule)
        return;

    debug("\r\nSCHEDULE for seq %ld:\r\n", seq);

    uint32_t tier1_bitmap = 0, tier2_bitmap = 0;
    uint8_t tier1_size = 0;

    for(struct node_info *n = nodes; n < nodes + nodes_count; n++) {
        uint32_t bitmap = get_tx_bitmap(n->mask, n->match);
        debug("%04x : tier %d mask %02x match %02x seq %9ld / %9ld rssi %3d btn %d\r\n",
                n->who, n->tier_one ? 1 : 2, n->mask, n->match, n->seq_1, n->seq_2, n->rssi, n->button_dist);
        if(n->tier_one) {
            if(tier1_bitmap & bitmap)
                debug("  collision!\r\n");
            tier1_bitmap |= bitmap;
            tier1_size++;
        } else
            tier2_bitmap |= bitmap;
    }

    uint8_t mask = 4;
    while(mask < tier1_size + 3) mask <<= 1;
    mask -= 1;

    uint32_t forbidden = tier1_bitmap | tier2_bitmap;

    debug("tier1: %08lx, tier2: %08lx, preferred mask: %02x\r\n",
          tier1_bitmap, tier2_bitmap, mask);

    uint8_t match, score;
    do
    {
        score = 0xff;
        for(uint8_t m = 0; m <= mask; m++)
        {
            uint32_t bitmap = get_tx_bitmap(mask, m);
            if(forbidden & bitmap)
                continue;
            uint8_t s = score_tx_bitmap(bitmap | tier1_bitmap);
            if(s < score) {
                match = m;
                score = s;
            }
        }
        if(score != 0xff)
            break;
        mask = (mask<<1) | 1;
        debug("sched: blowing up mask to %02x\r\n", mask);
    } while(mask <= 31);

    if(score == 0xff) {
        debug("sched: no mask / match found\r\n");
        until_schedule = SCHEDULE_PERIOD;
        return;
    }

    if(tx_mask == mask && tx_match == match)
        debug("no change; mask = %02x, match = %02x\r\n", tx_mask, tx_match, score);
    else {
        tx_mask = mask;
        tx_match = match;
        debug("settling on mask = %02x, match = %02x, score is %02x\r\n", tx_mask, tx_match, score);
    }
    until_schedule = (iinic_random_8() & (-SCHEDULE_PERIOD)) | SCHEDULE_PERIOD;
}

static void update_node(struct node_info *incomming)
{
    if(incomming->match & ~incomming->mask) {
        debug("sched: node %04x reports bad mask = %02x, match = %02x\r\n",
              incomming->who, incomming->mask, incomming->match);
        return;
    }

    struct node_info *n;
    for(n = nodes; n < nodes + nodes_count; n++)
        if(n->who == incomming->who)
            break;

    if(n == nodes + nodes_count) {
        if(nodes_count == NODES_COUNT) {
            debug("sched: no room for new node\r\n");
            return;
        }
        *n = *incomming;
        n->sent = 0;
        nodes_count++;
        nodes_changed = true;

        debug("sched: new node %04x has mask = %02x, match = %02x, tier = %d\r\n",
               n->who, n->mask, n->match, n->tier_one ? 1 : 2);
        return;
    }

    if(n->tier_one && !incomming->tier_one)
        return;

    if(incomming->tier_one && !n->tier_one) {
        debug("sched: node %04x promoted to tier one\r\n", n->who);
        n->tier_one = true;
        n->sent = 0;
        nodes_changed = true;
    }

    if(n->tier_one) {
        n->seq_1 = incomming->seq_1;
        n->rssi = incomming->rssi;
    }
    n->seq_2 = incomming->seq_2;
    n->button_dist = incomming->button_dist;
    n->sent = 0;

    if(n->mask != incomming->mask || n->match != incomming->match) {
        n->mask = incomming->mask;
        n->match = incomming->match;
        debug("sched: node %04x changed: mask = %02x, match = %02x\r\n",
              n->who, n->mask, n->match);
        nodes_changed = true;
    }
}

/* ------------------------------------------------------------------------- */

static void receive(uint8_t *rxbuf, uint16_t rxlen, uint16_t rssi, const round_timing *rxrt)
{
    struct packet_hdr *hdr = (struct packet_hdr *) rxbuf;

    if(rxlen < sizeof(struct packet_hdr)) {
        //debug("rx: truncated (header+crc)\r\n");
        return;
    }
    if(rxlen <  hdr->length) {
        //debug("rx: truncated (hdr->length)\r\n");
        return;
    }
    rxlen = hdr->length;

    if(!check_crc(rxbuf, rxlen)) {
        //debug("rx: bad crc\r\n");
        return;
    }

    int16_t round_offset_err = rxrt->round_offset - round_offset;
    if(0 == nodes_count || round_offset_err > ROUND_OFFSET_THRESHOLD || round_offset_err < -ROUND_OFFSET_THRESHOLD) {
        debug("time: updating round offset: %04x -> %04x\r\n", round_offset, rxrt->round_offset);
        round_offset = rxrt->round_offset;
    }

    int32_t seq = seq_delta + rxrt->seq;
    if(0 == nodes_count || seq < hdr->seq) {
        debug("time: updating round seq: %ld -> %ld\r\n", seq, hdr->seq);
        seq_delta = hdr->seq - rxrt->seq;
        seq = hdr->seq;
    }

    {
        struct node_info n;
        n.who = hdr->sender;
        n.seq_1 = n.seq_2 = seq;
        n.rssi = rssi;
        n.mask = mask_id_dec(hdr->mask_id);
        n.match = seq & n.mask;
        n.tier_one = true;
        n.button_dist = hdr->button_dist;
        update_node(&n);
    }

    struct packet_neighbour_info *ni =
        (struct packet_neighbour_info *)(rxbuf + sizeof(struct packet_hdr));
    struct packet_neighbour_info *niend = ni + hdr->neighbour_infos;
    for(; ni < niend; ni++)
    {
        if(ni->who == iinic_mac)
            continue;
        struct node_info n;
        n.who = ni->who;
        n.seq_1 = n.seq_2 = seq;
        n.rssi = 0;
        n.mask = mask_id_dec(ni->mask_id);
        n.match = ni->match;
        n.tier_one = false;
        n.button_dist = ni->button_dist;
        update_node(&n);
    }
}

static uint16_t transmit(uint8_t *txbuf, int32_t seq)
{
    if(!should_tx(seq))
        return 0;

    uint8_t neighbour_infos = 0;
    struct packet_neighbour_info *ni =
        (struct packet_neighbour_info *)(txbuf + sizeof(struct packet_hdr));

    for(struct node_info *n = nodes; n < nodes + nodes_count; n++) {
        if(!n->tier_one || n->sent)
            continue;
        ni->who = n->who;
        ni->match = n->match;
        ni->mask_id = mask_id_enc(n->mask);
        ni->button_dist = n->button_dist;
        n->sent = 1;
        ni++;
        neighbour_infos++;
    }

    struct packet_hdr *hdr = (struct packet_hdr *) txbuf;
    hdr->sender = iinic_mac;
    hdr->seq = 0 == nodes_count ? 0 : seq;
    hdr->length = sizeof(struct packet_hdr) + neighbour_infos * sizeof(struct packet_neighbour_info) + 2;
    hdr->neighbour_infos = neighbour_infos;
    hdr->_unused = 0;
    hdr->mask_id = mask_id_enc(tx_mask);
    hdr->button_dist = my_button_dist;

    overwrite_crc(txbuf, hdr->length);
    return hdr->length;
}

static void update_my_button_dist_and_connectivity()
{
    bool t1 = false;
    uint16_t bd = iinic_read_button() ? 0 : 15;

    for(struct node_info *n = nodes; n < nodes + nodes_count; n++)
        if(n->tier_one) {
            t1 = true;
            if(n->button_dist+1 < bd)
                bd = n->button_dist+1;
        }

    my_connectivity = t1 ? 2 : nodes_count ? 1 : 0;
    my_button_dist = bd;
}

ISR(TIMER0_COMP_vect)
{
    static uint8_t c;
    c = (c+1) & 63;

    uint8_t con = my_connectivity;
    if((1 == con && c < 4) || (2 == con && (c < 4 || (c > 8 && c < 16))))
        iinic_led_off(IINIC_LED_GREEN);
    else
        iinic_led_on(IINIC_LED_GREEN);

    static int8_t dist_blinks;
    if((c & 15) == 0) {
        if(-4 == dist_blinks)
            dist_blinks = (1+my_button_dist) & 15;
        if(dist_blinks > 0)
            iinic_led_on(IINIC_LED_RED);
        dist_blinks --;
    } else
        iinic_led_off(IINIC_LED_RED);
}

void __attribute__((noreturn)) iinic_main()
{
    /* timer0: ctc mode, F_CPU/1024, interrupts on clear (f = 1/64 s) */
    OCR0 = 224;
    TCCR0 = _BV(WGM01) | _BV(CS02) | _BV(CS00);
    TIMSK |= _BV(OCIE0);

    iinic_set_rx_knobs(IINIC_RSSI_91 | IINIC_GAIN_20 | IINIC_BW_270);
    iinic_set_tx_knobs(IINIC_POWER_75 | IINIC_DEVIATION_240);
    iinic_set_bitrate(IINIC_BITRATE_57600);

    iinic_usart_is_debug();
    debug("\r\n\r\nHello, world, my mac is %04x\r\n", iinic_mac);

    iinic_set_buffer(buf1, PACKET_MAX_LENGTH);
    iinic_rx();

    for(;;)
    {
        round_timing rt;
        iinic_get_now(&rt.t);

        { bool k = rt.round_offset > round_offset;
          rt.round_offset = round_offset;
          if(k) rt.seq ++; }

        int32_t seq = seq_delta + rt.seq;

        schedule(seq);
        seq = seq_delta + rt.seq;

        update_my_button_dist_and_connectivity();

        uint8_t *rxbuf = iinic_buffer;
        uint8_t *txbuf = buf1 == iinic_buffer ? buf2 : buf1;

        uint16_t txlen = transmit(txbuf, seq);

        uint8_t evt = iinic_timed_poll(IINIC_RX_COMPLETE, &rt.t);

        uint16_t rxlen = 0 == evt ? 0 : iinic_buffer_ptr - iinic_buffer;

        if(0 != txlen) {
            if(0 != rxlen)
                iinic_timed_poll(0, &rt.t);
            else
                iinic_idle();
            iinic_set_buffer(txbuf, txlen);
            iinic_tx();
        }

        uint16_t rssi;
        round_timing rxrt;
        if(0 != rxlen) {
            rxrt.t = iinic_rx_timing;
            rssi = iinic_rx_rssi;
        }

        if(0 == txlen && 0 != rxlen) {
            iinic_set_buffer(txbuf, PACKET_MAX_LENGTH);
            iinic_rx();
        }

        if(0 != rxlen)
            ;//debug("rx: %d bytes\r\n", rxlen);
        if(0 != txlen)
            ;//debug("tx: %d bytes\r\n", txlen);

        if(0 != rxlen) {
            receive(rxbuf, rxlen, rssi, &rxrt);
            iinic_timed_poll(0, &rt.t);
        }

        if(0 != txlen) {
            iinic_infinite_poll(IINIC_TX_COMPLETE);
            iinic_set_buffer(buf1, PACKET_MAX_LENGTH);
            iinic_rx();
        }
    }
}
