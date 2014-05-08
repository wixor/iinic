
import struct, time
import Config
from OurException import OurException
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

def idToStr(i):
    return struct.pack('H', i)

def strToId(s):
    return struct.unpack('H', s)[0]

class Frame:
    @staticmethod
    def lengthOverhead():
        return 2*Config.ID_LENGTH + 1 + 1 + 1
    
    def __init__(self):
        pass

    def fromReceived(self, msg, firstTiming, power):
        self._bytes = msg
        self._timing = firstTiming
        self._power = power

    def toSend(self, ftype, fromId, toId, payload):
        l = len(payload)
        if l > 255:
            raise OurException('Frame payload too long, maximum is 255')
        self._bytes = chr(l) + ftype + idToStr(fromId) + idToStr(toId) + payload
        self._bytes += chr(computeCRC_8(self._bytes))

    def bytes(self):
        return self._bytes
    def __repr__(self):
        return 'From:'+str(self.fromId())+' To:'+str(self.toId())+' Payload:'+self.content()
    def type(self):
        return self._bytes[1]
    def fromId(self):
        return strToId(self._bytes[2:2+Config.ID_LENGTH])
    def toId(self):
        return strToId(self._bytes[2+Config.ID_LENGTH:2+2*Config.ID_LENGTH])
    def content(self):
        return self._bytes[2+2*Config.ID_LENGTH:-1]
    def isValid(self):
        return self._bytes[-1] == chr(computeCRC_8(self._bytes[:-1]))
    def timing(self):
        return self._timing
    def power(self):
        return self._power
    def payloadLength(self):
        return ord(self._bytes[0])

class FrameLayer:
    def __init__(self, nic, myId = None):
        self.nic = nic
        self.myId = myId or self.nic.get_uniq_id()

    def getMyId(self):
        return self.myId

    def _receiveFrame(self, deadline = None): # deadline for first message
        rxbytes = self.nic.rx(deadline)
        if not rxbytes:
            return None
        
        length = ord(rxbytes.bytes[0]) + Frame.lengthOverhead()
        if len(rxbytes.bytes) < length:
            return None
        
        frame.fromReceived(rxbytes.bytes[0:length], rxbytes.timing, rxbytes.rssi)
        if not frame.isValid():
            return None
        
        return frame

    def receiveFrame(self, deadline = None):
        while not deadline or time.time() < deadline:
            frame = self._receiveFrame(deadline)
            if frame:
                return frame
        return None
    
    def sendFrame(self, ftype, fromId, toId, content, timing = None):
        frame = Frame()
        frame.toSend(ftype, fromId, toId, content)

        if ftype == 'E':
            frame.bytes = frame.bytes[:-1] # create invalid (too short) frame! just for testing purposes

        if timing:
            self.nic.timing(timing)
        
        self.nic.tx(frame.bytes())
        
    def sync(self, deadline = None):
        self.nic.sync(deadline)
        
    # TODO: set channel, set power, get approx timing, get uniq_id
        