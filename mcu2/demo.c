#include <avr/pgmspace.h>
#include <util/crc16.h>
#include <stdio.h>
#include <string.h>

#include "iinic.h"

typedef union {
    iinic_timing t;
    struct {
        uint16_t round_offset;
        uint32_t seq;
    };
} round_timing;

enum { ROUND_OFFSET_THRESHOLD = 512 }; /* 2 bytes */
static uint16_t round_offset;
static uint32_t seq_delta;

struct packet_hdr {
    uint16_t sender;
    uint32_t seq;
    uint8_t length;
    unsigned neighcount : 4;
    unsigned mask_id : 2;
    unsigned button : 1;
    unsigned _unused : 1;
};

enum { PACKET_MAX_LENGTH = 248 };
static uint8_t buf1[PACKET_MAX_LENGTH], buf2[PACKET_MAX_LENGTH];

struct edge_info {
    uint16_t from;
    uint16_t to;
    uint32_t seq;
    uint8_t rssi;
};

enum {
    PACKET_MAX_EDGES = (PACKET_MAX_LENGTH - sizeof(struct packet_hdr) - 2) / sizeof(struct edge_info),
    EDGE_SENT = 0x80000000,
    EDGES_COUNT = 64,
    EDGE_TTL = 150,
};
static struct edge_info edges[EDGES_COUNT];
static uint8_t edges_count;

struct neighbour {
    uint16_t who;
    uint8_t mask, match;
    uint8_t fails;
    uint8_t neighcount;
};
enum {
    NEIGHBOURS_COUNT = 16,
    NEIGHBOUR_FAILS_MAX = 16,
    SCHEDULE_PERIOD = 64

};
static struct neighbour neighbours[NEIGHBOURS_COUNT];
static uint8_t neighbours_count;
static uint8_t until_schedule;
static uint8_t tx_mask, tx_match;

/* ------------------------------------------------------------------------- */

static uint16_t compute_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xffff;
    while(len) {
        crc = _crc16_update(crc, *buf++);
        len--;
    }
    return crc;
}
static inline bool check_crc(const uint8_t *buf, uint16_t len) {
    return 0 == compute_crc(buf, len);
}
static inline void overwrite_crc(uint8_t *buf, uint16_t len) {
    *(uint16_t *)(buf+len-2) = compute_crc(buf, len-2);
}

/* ------------------------------------------------------------------------- */

static struct edge_info *oldest_edge(bool self_ok)
{
    struct edge_info *e, *oldest = NULL;
    uint32_t oldseq = 0xffffffff;

    for(e = edges; e < edges+edges_count; e++)
        if((self_ok || (e->from != iinic_mac && e->to != iinic_mac)) && e->seq < oldseq) {
            oldest = e;
            oldseq = e->seq;
        }

    return oldest;
}
static void update_edge(const struct edge_info *incomming)
{
    struct edge_info *e;

    for(e = edges; e < edges+edges_count; e++)
        if(e->from == incomming->from && e->to == incomming->to) {
            if((e->seq & ~EDGE_SENT) < (incomming->seq & ~EDGE_SENT)) {
                e->seq = incomming->seq;
                e->rssi = incomming->rssi;
                debug("edge: updating %04x -> %04x: seq %lu, rssi %d\r\n",
                      e->from, e->to, (e->seq & ~EDGE_SENT), e->rssi);
            } else {
                debug("edge: stale info for %04x -> %04x: seq %lu, has %lu\r\n",
                      e->from, e->to, (incomming->seq & ~EDGE_SENT), (e->seq & ~EDGE_SENT));
            }
            return;
        }

    if(edges_count < EDGES_COUNT)
        edges_count++;
    else {
        e = oldest_edge(false);
        debug("edge: recycling %04x -> %04x, seq %lu\r\n", e->from, e->to, (e->seq & ~EDGE_SENT));
    }
    *e = *incomming;
    debug("edge: creating %04x -> %04x: seq %lu, rssi %d\r\n", e->from, e->to, (e->seq & ~EDGE_SENT), e->rssi);
}

static void age_edges(uint32_t seq)
{
    uint32_t dead = seq - EDGE_TTL;
    for(struct edge_info *e = edges; e < edges + edges_count; )
        if((e->seq & ~EDGE_SENT) > dead)
            e++;
        else {
            debug("edge: dropping %04x -> %04x, seq %lu, too old\r\n", e->from, e->to, (e->seq & ~EDGE_SENT));
            *e = edges[--edges_count];
        }
}

/* ------------------------------------------------------------------------- */

static void do_schedule()
{
    if(until_schedule)
        return;
    until_schedule = (iinic_random_8() & (-SCHEDULE_PERIOD)) | SCHEDULE_PERIOD;

    uint8_t s = neighbours_count;
    for(struct neighbour *n = neighbours; n < neighbours + neighbours_count; n++)
        if(n->neighcount > s)
            s = n->neighcount;
    s += 3;

    uint8_t mask;
    if(0 == neighbours_count)
        mask = 31;
    else {
        mask = 4;
        while(mask < s) mask <<= 1;
        mask -= 1;
    }

    debug("sched: %d neighbours, s = %d, preferred mask: %02x\r\n", neighbours_count, s, mask);

    bool ok = false;
    uint8_t match;
    do {
        for(match = 0; match <= mask; match++)
        {
            ok = true;
            for(struct neighbour *n = neighbours; ok && n < neighbours + neighbours_count; n++)
                if((n->mask & mask & match) == (n->mask & mask & n->match))
                    ok = false;
            if(ok)
                break;
        }
        if(ok)
            break;
        mask = (mask<<1) | 1;
        debug("sched: blowing up mask to %02x\r\n", mask);
    } while(mask < 31);

    if(!ok) {
        debug("sched: failed to find mask/match\r\n");
        return;
    }

    debug("sched: settling on mask %02x, match %02x\r\n", mask, match);
    tx_mask = mask;
    tx_match = match;
}

static inline void schedule() {
    until_schedule--;
    do_schedule();
}

static void round_busy(uint8_t seq, uint16_t who, uint8_t mask, uint8_t neighcount)
{
    bool change = false;
    struct neighbour *nn = NULL;

    for(struct neighbour *n = neighbours; n < neighbours + neighbours_count; )
        if(n->who == who)
            nn = n++;
        else if((n->mask & seq) != n->match)
            n++;
        else if(++n->fails < NEIGHBOUR_FAILS_MAX)
            n++;
        else {
            debug("sched: neighbour %04x failed too many times, dropping\r\n", n->who);
            *n = neighbours[--neighbours_count];
            change = true;
        }

    if(0 != who && NULL == nn) {
        if(neighbours_count >= NEIGHBOURS_COUNT)
            debug("sched: no slot for new neighbour %04x\r\n", who);
        else {
            debug("sched: new neighbour %04x\r\n", who);
            nn = neighbours + (neighbours_count++);
            nn->who = who;
            nn->mask = 0;
        }
    }

    if(NULL != nn) {
        nn->fails = 0;
        if(nn->mask != mask || (seq & nn->mask) != nn->match || nn->neighcount != neighcount) {
            nn->mask = mask;
            nn->match = seq & mask;
            nn->neighcount = neighcount;
            change = true;
            debug("sched: neighbour %04x changed: mask %02x, match %02x, neighcount: %d\r\n",
                  who, nn->mask, nn->match, nn->neighcount);
        }
    }

    if(change && tx_mask != 0) {
        until_schedule >>= 1;
        do_schedule();
    }
}
static inline void round_idle(uint8_t seq) {
    round_busy(seq, 0, 0, 0);
}
static inline bool round_is_tx(uint8_t seq) {
    return 0 != tx_mask && (seq & tx_mask) == tx_match;
}

static uint8_t mask_id_enc(uint8_t mask)
{
    uint8_t ret = 0;
    while(mask > 3) {
        ret++;
        mask >>= 1;
    }
    return ret;
}

static uint8_t mask_id_dec(uint8_t mask_id)
{
    uint8_t ret = 4;
    while(mask_id) {
        ret <<= 1;
        mask_id--;
    }
    return ret-1;
}


/* ------------------------------------------------------------------------- */


static uint16_t transmit(uint8_t *buf, uint32_t seq)
{
    if(!round_is_tx(seq))
        return 0;

    struct edge_info *e = (struct edge_info *)(buf + sizeof(struct packet_hdr));
    uint8_t ecnt = 0;
    while(ecnt < PACKET_MAX_EDGES) {
        struct edge_info *oldest = oldest_edge(true);
        if(oldest->seq & EDGE_SENT)
            break;
        *e++ = *oldest;
        ecnt++;
        oldest->seq |= EDGE_SENT;
    }

    struct packet_hdr *hdr = (struct packet_hdr *) buf;
    hdr->sender = iinic_mac;
    hdr->seq = seq;
    hdr->length = sizeof(struct packet_hdr) + ecnt * sizeof(struct edge_info) + 2;
    hdr->neighcount = neighbours_count > 15 ? 15 : neighbours_count;
    hdr->mask_id = mask_id_enc(tx_mask);
    hdr->button = iinic_read_button();
    hdr->_unused = 0;

    overwrite_crc(buf, hdr->length);
    return hdr->length;
}

static void receive(uint8_t *rxbuf, uint16_t rxlen, uint16_t rssi, const round_timing *rxrt)
{
    struct packet_hdr *hdr = (struct packet_hdr *) rxbuf;

    if(rxlen < sizeof(struct packet_hdr) + 2 || hdr->length > rxlen) {
        debug("rx: truncated\r\n");
        return;
    }
    rxlen = hdr->length;

    if(!check_crc(rxbuf, rxlen)) {
        debug("rx: bad crc\r\n");
        return;
    }

    int16_t round_offset_err = rxrt->round_offset - round_offset;
    if(round_offset_err > ROUND_OFFSET_THRESHOLD || round_offset_err < -ROUND_OFFSET_THRESHOLD) {
        debug("time: updating round offset: %04x -> %04x\r\n", round_offset, rxrt->round_offset);
        round_offset = rxrt->round_offset;
    }

    uint32_t seq = seq_delta + rxrt->seq;
    if(seq < hdr->seq) {
        debug("time: updating round seq: %lu -> %lu\r\n", seq, hdr->seq);
        seq_delta = hdr->seq - rxrt->seq;
        seq = hdr->seq;
    }

    round_busy(seq, hdr->sender, mask_id_dec(hdr->mask_id), hdr->neighcount);

    struct edge_info *e = (struct edge_info *)(rxbuf + sizeof(struct packet_hdr)),
                     *ecap = (struct edge_info *)(rxbuf + rxlen - 2 - sizeof(struct edge_info));
    for(; e <= ecap; e++) {
        if(e->from == iinic_mac)
            continue;
        if(e->to == iinic_mac)
            e->seq |= EDGE_SENT;
        update_edge(e);
    }

    struct edge_info neighbour = {
        .from = iinic_mac,
        .to = hdr->sender,
        .seq = seq,
        .rssi = rssi/2
    };
    update_edge(&neighbour);
}

void __attribute__((noreturn)) iinic_main()
{
    iinic_set_rx_knobs(IINIC_RSSI_91 | IINIC_GAIN_20 | IINIC_BW_270);
    iinic_set_tx_knobs(IINIC_POWER_175 | IINIC_DEVIATION_240);
    iinic_set_bitrate(IINIC_BITRATE_57600);

    iinic_usart_is_debug();
    debug("\r\n\r\nHello, world, my mac is %04x\r\n", iinic_mac);

    until_schedule = SCHEDULE_PERIOD;

    iinic_set_buffer(buf1, PACKET_MAX_LENGTH);
    iinic_rx();

    for(;;)
    {
        round_timing rt;
        iinic_get_now(&rt.t);

        { bool k = rt.round_offset > round_offset;
          rt.round_offset = round_offset;
          if(k) rt.seq ++; }

        uint32_t seq = seq_delta + rt.seq;

        age_edges(seq-1);
        schedule();

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
            debug("rx: %d bytes\r\n", rxlen);
        if(0 != txlen)
            debug("tx: %d bytes\r\n", txlen);

        if(0 == rxlen)
            round_idle(seq);
        else {
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
