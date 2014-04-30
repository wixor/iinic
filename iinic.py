import math, time, socket, select, struct, collections

RxByte = collections.namedtuple('RxByte', ('byte', 'bitrate', 'channel', 'power', 'timing'))

class Token(object):
    ESCAPE = '\x5a'

    @classmethod
    def extract(cls, buf):
        data = struct.unpack(cls.FIELDFMT, buf[2:2+cls.TAIL])
        return cls(**dict(zip(cls.FIELDNAMES, data)))

    def __init__(self, **kwargs):
        for k,v in kwargs.iteritems():
            if k not in self.FIELDNAMES:
                raise TypeError('Unexpected keyword argument %s for token %s' % (k, type(self).__name__))
            setattr(self, k, v)

    def serialize(self):
        return self.__str__()

    def __str__(self):
        args = [ getattr(self, k) for k in self.FIELDNAMES ]
        data = struct.pack(self.FIELDFMT, *args)
        return self.ESCAPE + self.TAG + data

    def __repr__(self):
        return type(self).__name__ + \
            '(' + ', '.join(['%s = %r' % (k,getattr(self, k)) for k in self.FIELDNAMES ]) + ')'

class PlainByteToken(object):
    TAG = None
    FIELDFMT = ''
    FIELDNAMES = ()
    TAIL = 0
    LENGTH = 1

    @classmethod
    def extract(cls, buf):
        return PlainByteToken(buf[0])

    def __init__(self, byte):
        self.byte = byte
    
    def __repr__(self):
        return 'PlainByteToken(0x%02x)' % ord(self.byte)

    def serialize(self):
        return self.byte

def make_token(name, tag, fieldfmt='', fieldnames=()):
    class ret(Token):
        TAG = tag
        FIELDFMT = fieldfmt
        FIELDNAMES = fieldnames
        TAIL = (0 if fieldfmt == '' else struct.calcsize(fieldfmt))
        LENGTH = 2+TAIL
    ret.__name__ = name
    return ret

UnescapeToken = make_token('UnescapeToken', '\xa5')
ResetRqToken = make_token('ResetRqToken', '\x01')
ResetAckToken = make_token('ResetAckToken', '\x5a', '<BBIIH', ('channel', 'power', 'bitrate', 'timing_lo', 'timing_hi'))
ChannelTagToken = make_token('ChannelTagToken', '\x02', '<B', ('channel',))
PowerTagToken = make_token('PowerTagToken', '\x03', '<B', ('power',))
BitrateTagToken = make_token('BitrateTagToken', '\x04', '<I', ('bitrate',))
TimingTagToken = make_token('TimingTagToken', '\x05', '<IH', ('timing_lo','timing_hi'))
PingToken = make_token('PingToken', '\x06', '<I', ('seq', ))
TxToken = make_token('TxToken', '\x07')

allTokens = (
    UnescapeToken, ResetRqToken, ResetAckToken, ChannelTagToken,
    PowerTagToken, BitrateTagToken, TimingTagToken,
    PingToken, TxToken,
)

def extract_token(buf):
    if 0 == len(buf):
        return None

    if buf[0] != Token.ESCAPE:
        return PlainByteToken(buf[0])

    if len(buf) < 2:
        return None
    tag = buf[1]
    for t in allTokens:
        if tag == t.TAG:
            if len(buf) < t.LENGTH:
                return None
            return t.extract(buf)

    raise IOError('unrecognized token (tag 0x%02x)' % ord(tag))

class PingFuture(object):
    def __init__(self, nic, seq):
        self.nic = nic
        self.seq = seq
        self.acked = False

    def await(self, deadline=None):
        while not self.acked and self.nic._rx(deadline) is not None:
            pass
        return self.acked

class NIC(object):
    def __init__(self, comm, deadline=None):
        self._comm = comm
        self._pingseq = 0
        self.reset(deadline)

    def reset(self, deadline=None):
        self._pings = dict()
        self._rxbuf = ''

        self._comm.send(ResetRqToken().serialize())

        while True:
            e = self._rx(deadline)
            if e is None:
                raise IOError('failed to reset NIC in given time')
            if isinstance(e, ResetAckToken):
                break

        self._txbuf = ''
        self._txchannel = self._rxchannel = e.channel
        self._txbitrate = self._rxbitrate = e.bitrate
        self._txpower = self._last_txpower = e.power
        self._rxtiming = e.timing_lo + (e.timing_hi<<32)
        self._rxbytes = 0
        self._rxpower = None
        self._rxqueue = []

    def ping(self):
        seq = self._pingseq = self._pingseq+1
        self._pings[seq] = future = PingFuture(self, seq)
        self._comm.send(PingToken(seq=seq).serialize())
        return future

    def sync(self, deadline=None):
        self.ping().await(deadline)

    def timing(self, timing):
        self._comm.send(TimingTagToken(
            timing_lo = timing & (1<<32)-1,
            timing_hi = timing >> 32
        ).serialize())

    def set_channel(self, channel):
        if self._txchannel == channel:
            return
        self._txchannel = channel
        self._comm.send(ChannelTagToken(channel=channel).serialize())

    def set_bitrate(self, bitrate):
        if self._txbitrate == bitrate:
            return
        self._txbitrate = bitrate
        self._comm.send(BitrateTagToken(bitrate=bitrate).serialize())

    def set_power(self, power):
        self._txpower = power

    def tx(self, payload):
        if not payload:
            return

        if self._txpower != self._last_txpower:
            self._last_txpower = self._txpower
            self._comm.send(PowerTagToken(power=self._txpower).serialize())

        self._comm.send(
            payload.replace(Token.ESCAPE, Token.ESCAPE + UnescapeToken.TAG) +
            TxToken().serialize()
        )

    def _nextToken(self, deadline = None):
        while True:
            e = extract_token(self._rxbuf)
            if e is not None:
                self._rxbuf = self._rxbuf[e.LENGTH:]
                return e

            rx = self._comm.recv(deadline)
            if not rx:
                return None
            self._rxbuf += rx

    def _rxbyte(self, b):
        rxbytes = self._rxbytes
        self._rxbytes += 1
        return RxByte(byte = b,
            bitrate = self._rxbitrate,
            channel = self._rxchannel,
            power = self._rxpower,
            timing = int(self._rxtiming + (rxbytes-1) * 8e+6 / self._rxbitrate)
        )

    def _rx(self, deadline = None):
        e = self._nextToken(deadline)
        if e is None:
            pass
        elif isinstance(e, ResetAckToken):
            pass
        elif isinstance(e, UnescapeToken):
            self._rxqueue.append(self._rxbyte(Token.ESCAPE))
        elif isinstance(e, PlainByteToken):
            self._rxqueue.append(self._rxbyte(e.byte))
        elif isinstance(e, ChannelTagToken):
            self._rxchannel = e.channel
        elif isinstance(e, PowerTagToken):
            self._rxpower = e.power
        elif isinstance(e, BitrateTagToken):
            self._rxbitrate = e.bitrate
        elif isinstance(e, TimingTagToken):
            self._rxtiming = e.timing_lo + (e.timing_hi<<32)
            self._rxbytes = 0
        elif isinstance(e, PingToken):
            if e.seq in self._pings:
                self._pings[e.seq].acked = True
                del self._pings[e.seq]
        else:
            raise IOError('unexpected token received from NIC: %r' % e)

        return e

    def rx(self, deadline = None):
        while True:
            if len(self._rxqueue) > 0:
                b = self._rxqueue[0]
                self._rxqueue = self._rxqueue[1:]
                return b

            e = self._rx(deadline)
            if e is None:
                return None

class NetComm(object):
    def __init__(self, host='themis.lo14.wroc.pl', port=2048):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self._sock.connect((host, port))
        self._poll = select.poll()
        self._poll.register(self._sock.fileno(), select.POLLIN)

    def recv(self, deadline = None):
        if deadline is None:
            timeout = None
        elif deadline == 0:
            timeout = 0
        else:
            timeout = max(0, deadline - time.time())
            timeout = int(math.ceil(1000. * timeout))

        if not self._poll.poll(timeout):
            return None
        rx = self._sock.recv(4096)
        if 0 == len(rx):
            raise IOError('lost comm')
        return rx

    def send(self, data):
        self._sock.send(data)

    def fileno(self):
        return self._sock.fileno()

