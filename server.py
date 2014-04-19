#!/usr/bin/python
import math, collections, heapq, logging, time, socket, select, errno

from iinic import extract_token, \
    PlainByteToken, \
    UnescapeToken, ResetRqToken, ResetAckToken, ChannelTagToken, \
    PowerTagToken, BitrateTagToken, TimingTagToken, \
    PingToken, TxToken

TxByte = collections.namedtuple('TxByte', ('client', 'byte', 'channel', 'power', 'bitrate'))

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
        self.resetState()
        logging.info('peer %s is connected', self.peer)

    def resetState(self):
        self.channel = 1
        self.power = 10
        self.bitrate = 300
        self.treset = self.server.time
        self.tlasttiming = 0
        self.broadcastbytes = 0
        self.rxbuf = ''
        self.cmdqueue = []
        self.txqueue = ''
        self.txremaining = 0

    @property
    def timing(self):
        return int(math.ceil(1.e6 * (self.server.time - self.treset)))

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
                self.queueTxByte(e.byte)
            elif isinstance(e, UnescapeToken):
                self.queueTxByte(e.ESCAPE)
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

    def queueTxByte(self, b):
        if len(self.txqueue) == 765:
            logging.warning('peer %s hit txqueue length limit', self.peer)
        if len(self.txqueue) < 768:
            self.txqueue += b

    def runCommand(self):
        if 0 == len(self.cmdqueue):
            return
        e = self.cmdqueue[0]
        if isinstance(e, ResetRqToken):
            self.resetCommand(e)
        elif isinstance(e, ChannelTagToken):
            self.setChannelCommand(e)
        elif isinstance(e, PowerTagToken):
            self.setPowerCommand(e)
        elif isinstance(e, BitrateTagToken):
            self.setBitrateCommand(e)
        elif isinstance(e, TimingTagToken):
            self.timingCommand(e)
        elif isinstance(e, PingToken):
            self.pingCommand(e)
        elif isinstance(e, TxToken):
            self.txCommand(e)

    def nextCommand(self):
        self.cmdqueue = self.cmdqueue[1:]
        self.runCommand()

    def resetCommand(self, e):
        self.resetState()
        
        timing = self.timing
        self.send(ResetAckToken(
            channel = self.channel,
            power = self.power,
            bitrate = self.bitrate,
            timing_lo = timing & (1<<32)-1,
            timing_hi = timing >> 32
        ).serialize())

        self.nextCommand()

    def setChannelCommand(self, e):
        if not (0 <= e.channel < 32):
            logging.warning('peer %s tries to use invalid channel %d', self.peer, e.channel)
        else:
            self.channel = e.channel
            self.send(e.serialize())
        self.nextCommand()

    def setPowerCommand(self, e):
        self.power = e.power
        self.nextCommand()

    def setBitrateCommand(self, e):
        if not (300 <= e.bitrate <= 115200):
            logging.warning('peer %s tries to set invalid bitrate %d', self.peer, e.bitrate)
        else:
            self.bitrate = e.bitrate
            self.send(e.serialize())
        self.nextCommand()

    def timingCommand(self, e):
        timing = self.timing
        etiming = (e.timing_hi << 32) | e.timing_lo

        if etiming <= timing:
            self.nextCommand()
            return

        if etiming - timing > 30000000:
            logging.warning('peer %s tries to use very large delay: %d', self.peer, (etiming - timing))
            self.nextCommand()
            return

        t = self.server.time + 1e-6 * (etiming - timing)
        self.server.queue(t, self.nextCommand)
    
    def pingCommand(self, e):
        self.send(e.serialize())
        self.nextCommand()

    def txCommand(self, e):
        self.txremaining = len(self.txqueue)
        self.txNext()

    def txNext(self):
        if 0 == self.txremaining:
            self.nextCommand()
            return
        b = self.txqueue[0]
        self.txqueue = self.txqueue[1:]
        self.txremaining -= 1
        self.server.tx(TxByte(
            client = self,
            byte = b,
            channel = self.channel,
            power = self.power,
            bitrate = self.bitrate
        ), self.txNext)

    def broadcast(self, tx):
        if tx.client is self or tx.channel != self.channel or tx.bitrate != self.bitrate:
            return

        self.broadcastbytes += 1
        expectedtime = self.tlasttiming + self.broadcastbytes * 8. / self.bitrate
        if abs(expectedtime - self.server.time) >= 6. / self.bitrate:
            self.broadcastbytes = 0
            self.tlasttiming = self.server.time
            timing = self.timing
            self.send(TimingTagToken(
                timing_lo = timing & (1<<32)-1,
                timing_hi = timing >> 32
            ).serialize())

        if tx.byte == UnescapeToken.ESCAPE:
            self.send(UnescapeToken().serialize())
        else:
            self.send(tx.byte)

### ----------------------------------------------------------------------- ###

class Channel(object):
    def __init__(self, nr, server):
        self.nr = nr
        self.server = server
        self.currtx = None
        self.txend = 0 

    def tx(self, tx, cb):
        txend = self.server.time + 8./tx.bitrate
        self.currtx = tx if self.txend <= self.server.time else None
        self.txend = max(self.txend, txend)
        def onend():
            if self.currtx is tx:
                self.currtx = None
                self.server.broadcast(tx)
            cb()
        self.server.queue(txend, onend)

### ----------------------------------------------------------------------- ###

class Server(object):
    def __init__(self, host, port):
        self.channels = [Channel(i, self) for i in xrange(32)]
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
        self.channels[tx.channel].tx(tx, cb)
    def broadcast(self, tx):
        for c in self.clients:
            c.broadcast(tx)

    def loop(self):
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
    logging.basicConfig(
        format = '%(asctime)s %(message)s',
        datefmt = '%Y-%m-%d %H:%M:%S',
        level = logging.INFO
    )
    Server('0.0.0.0', 2048).loop()

