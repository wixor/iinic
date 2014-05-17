#!/usr/bin/python
import math, random, collections, heapq, logging, time, socket, select, errno

from iinic import extract_token, \
    PlainByteToken, \
    UnescapeToken, ResetRqToken, ResetAckToken, \
    SetRxKnobsToken, SetPowerToken, SetBitrateToken, \
    TimingToken, PingToken, TxToken, RxToken \

TxBytes = collections.namedtuple('TxBytes', ('client', 'bytes', 'frequency', 'bitrate', 'duration'))
MyTxToken = collections.namedtuple('MyTxToken', ('buf'))

### ----------------------------------------------------------------------- ###

class Listener(object):
    def __init__(self, host, port, handler):
        self.handler = handler
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((host, port))
        self.socket.listen(5)

    def recv(self):
        (newsock, _) = self.socket.accept()
        self.handler.connected(newsock)

    def fileno(self):
        return self.socket.fileno()

### ----------------------------------------------------------------------- ###

class Client(object):
    def __init__(self, socket, server):
        self.socket = socket
        self.server = server
        self.peer = '%s:%d' % socket.getpeername()
        self.connected = True
        self.uniq_id = random.randint(1,65534)
        self.resetState()
        logging.info('peer %s is connected', self.peer)

    @staticmethod
    def time2timing(x):
        return int(math.ceil(x * 1843200.))
    @staticmethod
    def timing2time(x):
        return 0.00000054253472*x

    def resetState(self):
        self.frequency = None
        self.power = None
        self.bitrate = None
        self.treset = self.server.time
        self.rxbuf = ''
        self.cmdqueue = []
        self.txqueue = ''

    def fileno(self):
        return self.socket.fileno()

    def disconnected(self):
        logging.info('peer %s is disconnected', self.peer)
        self.connected = False
        self.server.disconnected(self)
        self.socket.close()

    def recv(self):
        try:
            rx = self.socket.recv(4096)
        except IOError as err:
            logging.error('exception while receiving from peer %s: %r', self.peer, err)
            rx = ''

        if '' == rx:
            self.disconnected()
            return

        emptyQueue = len(self.cmdqueue) == 0

        self.rxbuf += rx
        while True:
            e = extract_token(self.rxbuf)
            if e is None:
                break
            self.rxbuf = self.rxbuf[e.LENGTH:]
            if isinstance(e, PlainByteToken):
                self.txqueue += e.byte
            elif isinstance(e, UnescapeToken):
                self.txqueue += e.ESCAPE
            elif isinstance(e, TxToken):
                self.cmdqueue.append(MyTxToken(self.txqueue))
                self.txqueue = ''
            else:
                self.cmdqueue.append(e)

        if emptyQueue and len(self.cmdqueue) > 0:
            self.runCommand()

    def send(self, m):
        if not self.connected:
            return
        try:
            self.socket.send(m)
        except IOError as err:
            logging.error('exception while sending to peer %s: %r', self.peer, err)
            self.disconnected()

    def runCommand(self):
        if 0 == len(self.cmdqueue):
            return
        e = self.cmdqueue[0]
        if isinstance(e, ResetRqToken):
            self.resetCommand(e)
        elif isinstance(e, SetRxKnobsToken):
            self.setRxKnobsCommand(e)
        elif isinstance(e, SetPowerToken):
            self.setPowerCommand(e)
        elif isinstance(e, SetBitrateToken):
            self.setBitrateCommand(e)
        elif isinstance(e, TimingToken):
            self.timingCommand(e)
        elif isinstance(e, PingToken):
            self.pingCommand(e)
        elif isinstance(e, MyTxToken):
            self.txCommand(e)

    def nextCommand(self):
        self.cmdqueue = self.cmdqueue[1:]
        self.runCommand()

    def resetCommand(self, e):
        logging.info('peer %s :: reset', self.peer)
        self.resetState()
        
        def ack():
            self.send(ResetAckToken(
                version_high = 1,
                version_low = 0,
                uniq_id = self.uniq_id
            ).serialize())
            self.nextCommand()

        self.server.queue(self.server.time + 0.5, ack)

    def setRxKnobsCommand(self, e):
        erssi = e.rx_knobs & 7
        if   erssi == 0: rssi = -103
        elif erssi == 1: rssi = -97
        elif erssi == 2: rssi = -91
        elif erssi == 3: rssi = -85
        elif erssi == 4: rssi = -79
        elif erssi == 5: rssi = -73
        else:
            logging.info('peer %s sent invalid rx_knobs (0x%02X)', self.peer, e.rx_knobs)

        egain = (e.rx_knobs >> 3) & 3
        if   egain == 0: gain = 0
        elif egain == 1: gain = -6
        elif egain == 2: gain = -14
        elif egain == 3: gain = -20

        ebw = (e.rx_knobs >> 5) & 7
        if   ebw == 1: bw = 400
        elif ebw == 2: bw = 340
        elif ebw == 3: bw = 270
        elif ebw == 4: bw = 200
        elif ebw == 5: bw = 134
        elif ebw == 6: bw = 67
        else:
            logging.info('peer %s sent invalid rx_knobs (0x%02X)', self.peer, e.rx_knobs)

        logging.info('peer %s :: setRxKnobs (freq %.3fMHz, deviation %dkHz, bandwidth %dkHz, rssi %ddB, gain %ddB)',
            self.peer, 20. * (43. + e.frequency / 4000), 15*(1+e.deviation), bw, rssi, gain)
        self.frequency = e.frequency
        self.nextCommand()

    def setPowerCommand(self, e):
        logging.info('peer %s :: setPower (%.1fdB)', self.peer, -2.5*e.power)
        self.power = e.power
        self.nextCommand()

    def setBitrateCommand(self, e):
        bitrate = 10000000. / 29. / (8. if 0 != (e.bitrate & 0x80) else 1.) / (1. + (e.bitrate & 0x7F))
        logging.info('peer %s :: setBitrate (%.3f)', self.peer, bitrate)
        self.bitrate = bitrate
        self.nextCommand()

    def timingCommand(self, e):
        t = self.timing2time((e.timing_hi << 16) | e.timing_lo)
        logging.info('peer %s :: timing (%.6f)', self.peer, t)
        t += self.treset

        if t <= self.server.time:
            logging.info('peer %s timing in the past', self.peer)
            self.nextCommand()
        else:
            self.server.queue(t, self.nextCommand)
    
    def pingCommand(self, e):
        logging.info('peer %s :: ping (%d)', self.peer, e.seq)
        self.send(e.serialize())
        self.nextCommand()

    def txCommand(self, e):
        logging.info('peer %s :: tx (%d bytes)', self.peer, len(e.buf))

        if self.frequency is None or \
           self.power is None or \
           self.bitrate is None:
            logging.warning('peer %s attemped tx on unconfigured radio!', self.peer)
            self.disconnected()
            return

        self.server.tx(TxBytes(
            client = self,
            bytes = e.buf,
            frequency = self.frequency,
            bitrate = self.bitrate,
            duration = (len(e.buf)+4) * 8. / self.bitrate
        ), self.nextCommand)

    def broadcast(self, tx):
        if tx.client is self or \
           tx.frequency != self.frequency or \
           tx.bitrate != self.bitrate:
            return
        if self.frequency is None or \
           self.power is None or \
           self.bitrate is None:
            return

        timing = self.server.time - tx.duration + 5.*8./tx.bitrate - self.treset
        if timing <= 0:
            return
        timing = self.time2timing(timing)

        logging.info('peer %s :: rx (%d bytes)', self.peer, len(tx.bytes))

        self.send(tx.bytes.replace(UnescapeToken.ESCAPE, UnescapeToken().serialize()))
        self.send(RxToken(
            timing_lo = timing & 0xFFFF,
            timing_hi = timing >> 16,
            rssi = 0
        ).serialize())

### ----------------------------------------------------------------------- ###

class LargePacketCollider(object):
    def __init__(self, frequency, server):
        self.frequency = frequency
        self.server = server
        self.currtx = None
        self.txend = 0 

    def tx(self, tx, cb):
        txend = self.server.time + tx.duration
        self.currtx = tx if self.txend <= self.server.time else None
        self.txend = max(self.txend, txend)
        def onend():
            if self.currtx is tx:
                self.currtx = None
                self.server.broadcast(tx)
            else:
                logging.info('tx of %d bytes from peer %s collided', len(tx.bytes), tx.client.peer)
            cb()
        self.server.queue(txend, onend)

### ----------------------------------------------------------------------- ###

class TimestampingFilter(logging.Filter):
    def __init__(self, server):
        super(TimestampingFilter, self).__init__()
        self.server = server
    def filter(self, record):
        record.servertime = self.server.time
        return super(TimestampingFilter, self).filter(record)

class Server(object):
    def __init__(self, host, port):

        logging.basicConfig(
            format = '%(servertime)f %(message)s',
            level = logging.INFO
        )

        self.time = time.time()
        logging.getLogger().addFilter(TimestampingFilter(self))

        logging.info("starting up...")
        self.colliders = dict()
        self.listener = Listener(host, port, self)
        self.clients = []
        self.heap = []
        
        self.polldict = dict()
        self.poll = select.poll()
        self.poll.register(self.listener, select.POLLIN)
        self.polldict[self.listener.fileno()] = self.listener

    def connected(self, sock):
        client = Client(sock, self)
        self.poll.register(client, select.POLLIN)
        self.polldict[client.fileno()] = client
        self.clients.append(client)

    def disconnected(self, client):
        self.clients.remove(client)
        del self.polldict[client.fileno()]
        self.poll.unregister(client)

    def queue(self, t, cb):
        heapq.heappush(self.heap, (t,cb))

    def tx(self, tx, cb):
        if tx.frequency not in self.colliders:
            self.colliders[tx.frequency] = LargePacketCollider(tx.frequency, self)
        self.colliders[tx.frequency].tx(tx, cb)
    def broadcast(self, tx):
        logging.info('broadcasting transmission from peer %s (%d bytes)',
            tx.client.peer, len(tx.bytes))
        for c in self.clients:
            c.broadcast(tx)

    def loop(self):
        logging.info("entering main loop...")

        evts = []
        while True:
            now = time.time()
            while len(self.heap) > 0 and now >= self.heap[0][0]:
                self.time, cb = heapq.heappop(self.heap)
                cb()
            self.time = now

            for fd, event in evts:
                self.polldict[fd].recv()

            timeout = int(math.ceil(1000. * (self.heap[0][0] - self.time))) \
                if len(self.heap) > 0 else None
            evts = self.poll.poll(timeout)

if __name__ == '__main__':
    Server('0.0.0.0', 2048).loop()

