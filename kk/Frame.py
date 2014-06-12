
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

    def toSend(self, ftype, fromId, toId, payload, timingIncluded=None):
        payload = str(payload)
        l = len(payload)
        
        if l > 255:
            raise OurException('Frame payload too long, maximum is 255')
        
        if l > 255 - 8 or timingIncluded == None: # We attach the timestamp to any frame we can.
            self._timestampTag = 0
        else:
            self._timestampTag=128 # TODO: ?
            l += 8
            
        self._bytes = chr(l) + chr(self._timestampTag ^ ord(ftype)) + idToStr(fromId) + idToStr(toId) + payload
        if self._timestampTag > 0:
            self._bytes += struct.pack('Q', timingIncluded) # 8 bytes
        self._bytes += chr(computeCRC_8(self._bytes))

    def bytes(self):
        return self._bytes
    def __repr__(self):
        if self.hasTime():
            return 'From:%5d To:%5d Type:%3d(%c) Payload:%s Included timing:%6d' % (self.fromId(), self.toId(), ord(self.type()), self.type(), self.payload(), self.recvTiming())
        else:
            return 'From:%5d To:%5d Type:%3d(%c) Payload:%s' % (self.fromId(), self.toId(), ord(self.type()), self.type(), self.payload())
    def type(self): # without timestamp tag
        return chr(ord(self._bytes[1]) & 127)
    def hasTime(self):
        return ord(self._bytes[1]) > 127
    def fromId(self):
        return strToId(self._bytes[2:2+Config.ID_LENGTH])
    def toId(self):
        return strToId(self._bytes[2+Config.ID_LENGTH:2+2*Config.ID_LENGTH])
    def content(self):
        end = -1
        if self.hasTime():
            end = end - 8
        return self._bytes[2+2*Config.ID_LENGTH:end]
    def payload(self):
        return self.content()
    def isValid(self):
        return self._bytes[-1] == chr(computeCRC_8(self._bytes[:-1]))
    def timing(self):
        return self._timing
    def recvTiming(self):
        return struct.unpack('Q', self._bytes[-9:-1])[0] if self.hasTime() else None
    def power(self):
        return self._power
    def payloadLength(self):
        return self.totalLength() - (8 if self.hasTime() else 0)
    def totalLength(self):
        return ord(self._bytes[0])

class FrameLayer:
    def __init__(self, nic, myId = None):
        self.nic = nic
        self.myId = myId or self.nic.get_uniq_id()
        
    def getNetworkTimeOffset(self):
        try:
            diff = self.timeManager.getNetworkTimeOffset()
            return diff or 0
        except:
            return 0
        

    def getMyId(self):
        return self.myId

    def _receiveFrame(self, deadline = None): # deadline for first message
        rxbytes = self.nic.rx(deadline)
        if not rxbytes:
            return None
        
        length = ord(rxbytes.bytes[0]) + Frame.lengthOverhead()
        if len(rxbytes.bytes) < length:
            return None
       
        frame = Frame()
        frame.fromReceived(rxbytes.bytes[0:length], int(rxbytes.timing-5000000.0*self.get_byte_send_time()), rxbytes.rssi)

        if not frame.isValid():
            return None
        
        try:
            self.timeManager.frameReceived(frame)
        except OurException as e:
            print >> sys.stderr, 'Warning, exception %s' % e

        return frame

    def receiveFrame(self, deadline = None):
        while not deadline or time.time() < deadline:
            frame = self._receiveFrame(deadline)
            if frame:
                return frame
        return None
    
    def sendFrame(self, ftype, fromId, toId, content, timing = None):
        frame = Frame()

        if timing:
            #This might help combat the innacuracies of the get_approx_timing() method.
            includeTiming = timing + self.getNetworkTimeOffset()
            frame.toSend(ftype, fromId, toId, content, includeTiming)
            self.nic.timing(timing)
            return self.nic.tx(frame.bytes())
        else:
            #We definitely do not want to attach a timestamp here.
            #Since we want this frame to be sent *now*, we don't know when exactly it will be sent
            #and the timestamp attached must be accurate (why inaccurate timestamps?)
            #get_approx_timing() function doesn't help here.
            frame.toSend(ftype, fromId, toId, content, None)
            return self.nic.tx(frame.bytes())

    # do not use it in protocols
    def _sync(self, deadline = None):
        self.nic.sync(deadline)
        
    def set_bitrate(self, bitrate):
        self.nic.set_bitrate(bitrate)
        
    def set_channel(self, channel):
        self.nic.set_channel(channel)
        
    # advanced
    def set_power(self, power):
        self.nic.set_power(power)
        
    # advanced
    def set_sensitivity(self, gain, rssi):
        self.nic.set_sensitivity(self, gain, rssi)
        
    def get_byte_send_time(self):
        bps = 43103.448 / (1.0+self.nic._bitrate)
        return 1.0 / bps
        
