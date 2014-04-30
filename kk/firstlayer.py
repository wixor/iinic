import sys, time, struct

from .. import iinic

def computeCRC_8(data, pattern = '0xD5'):
    div = 256 + int(pattern, 16)
    x = 0
    for c in data:
        x = (x<<8) + ord(c)
        for i in [7,6,5,4,3,2,1,0]:
            if x >> (i+8):
                x ^= div << i
        assert x < div
    return x

class FrameType:
    SYNC = 'S'
    DATA = 'D'
    ERROR = 'E'
    BROADCAST = 'B'
    SLAVE_REQUEST = 'R'
    # and some other?

# TODO: support IDs longer than 1 byte...
def idToStr(i, idLength = 1):
    return chr(i)

def strToId(s):
    return ord(s)

class Frame:
    def __init__(self):
        pass

    def fromReceived(self, msg, firstTiming, powers):
        self._bytes = msg
        self._timing = firstTiming
        self._power = sum(powers)/len(powers) if powers and powers[0] else None

    def toSend(self, ftype, fromId, toId, data):
        l = len(data)+5
        if l > 255:
            raise Exception('Frame too long, maximum is 255')
        self._bytes = chr(l) + ftype + idToStr(fromId, 1) + idToStr(toId, 1) + data
        self._bytes += chr(computeCRC_8(self._bytes))

    def str(self):
        return self._bytes
    def __repr__(self):
        return self._bytes[1:]
    def type(self):
        return self._bytes[1]
    def fromId(self):
        return strToId(self._bytes[2])
    def toId(self):
        return strToId(self._bytes[3])
    def content(self):
        return self._bytes[4:-1]
    def isValid(self):
        return self._bytes[-1] == chr(computeCRC_8(self._bytes[:-1]))
    def timing(self):
        return self._timing
    def power(self):
        return self._power

class FirstLayer:
    def __init__(self, nic, myId, timingVariance = 10, innerDeadline = 1.0):
        self.myId = myId
        self.nic = nic
        self.timingVariance = timingVariance
        self.buff = None
        self.innerDeadline = innerDeadline

    def _nextByte(self, deadline):
        if self.buff:
            x = self.buff
            self.buff = None
            assert x
            return x
        byte = self.nic.rx(deadline)
        return byte

    def _unnext(self, byte):
        assert not self.buff
        self.buff = byte

    def _receiveFrame(self, deadline = None): # deadline for first message
        expectDiff = 1000000/self.nic._txbitrate*8 + self.timingVariance
        message = ''

        first = self._nextByte(deadline)
        if not first:
            return None
        message += first.byte
        powers = [first.power]

        lastTime = first.timing
        for i in xrange(ord(first.byte)-1):
            rxbyte = self._nextByte(time.time() + self.innerDeadline)
            if not rxbyte:
                return None
            if rxbyte.timing - lastTime > expectDiff:
                # maybe it's a byte from the next frame? undo 'next'
                self._unnext(rxbyte)
                return None
            lastTime = rxbyte.timing
            message += rxbyte.byte
            powers += [rxbyte.power]
        frame = Frame()
        frame.fromReceived(message, first.timing, powers)
        if not frame.isValid():
            return None
        return frame

    def receiveFrame(self, deadline = None):
        while not deadline or time.time() < deadline:
            frame = self._receiveFrame(deadline)
            if frame:
                return frame
        return None

    def sendFrame(self, ftype, fromId, toId, content, timestamp = None):
        frame = Frame()
        frame.toSend(ftype, fromId, toId, content)

        if ftype == 'E':
            frame.bytes = frame.bytes[:-1] # create invalid (too short) frame! just for testing purposes

        if timestamp:
            self.nic.timing(timestamp)
            self.nic.tx(frame.str())

    def sync(self, deadline = None):
        self.nic.sync(deadline)

    # TODO: set channel, bitrate, power
