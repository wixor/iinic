
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

class Frame(dict):
    MANDATORY_FIELDS = ['ftype', 'fromId', 'toId', 'payload']
    ALL_FIELDS = MANDATORY_FIELDS + ['networkTime']
    
    @staticmethod
    def lengthOverhead():
        return 2*Config.ID_LENGTH + 1 + 1 + 1
    
    def __init__(self, fields = {}):
        dict.__init__(self, fields)

    def fromReceived(self, msg, firstTiming, power):
        length = ord(msg[0]) + Frame.lengthOverhead()
        if len(msg) < length:
            return False
       
        self._bytes = msg
        self._timing = firstTiming
        self._power = power
        
        self['ftype'] = chr(ord(self._bytes[1]) & 127)
        self['fromId'] = strToId(self._bytes[2:2+Config.ID_LENGTH])
        self['toId'] = strToId(self._bytes[2+Config.ID_LENGTH:2+2*Config.ID_LENGTH])
        end = -1
        if self.hasTime():
            end = end - 8
        self['payload'] = self._bytes[2+2*Config.ID_LENGTH:end]
        self['networkTime'] = struct.unpack('Q', self._bytes[-9:-1])[0] if self.hasTime() else None
        return self.isValid()
    
    def __getattr__(self, name):
        if name in Frame.ALL_FIELDS:
            return lambda: self.get(name, None)
    
    def totalLength(self):
        return ord(self._bytes[0])
    def payloadLength(self):
        return len(self.payload())
    def hasTime(self):
        return ord(self._bytes[1]) > 127
    def isValid(self):
        return self._bytes[-1] == chr(computeCRC_8(self._bytes[:-1]))
    def timing(self):
        return self._timing        
    def power(self):
        return self._power

    def toSend(self):
        for f in Frame.MANDATORY_FIELDS:
            if f not in self:
                raise OurException('Field %s is mandatory' % f)
        payload = str(self.payload())
        l = len(self.payload())
        
        if l > 255:
            raise OurException('Frame payload too long, maximum is 255')
        
        networkTime = self.networkTime()
        
        if l > 255 - 8 or networkTime == None: # We attach the timestamp to any frame we can.
            self._timestampTag = 0
        else:
            self._timestampTag=128 # TODO: ?
            l += 8
            
        string = chr(l) + chr(self._timestampTag ^ ord(self.ftype())) + idToStr(self.fromId()) + idToStr(self.toId()) + self.payload()
        if self._timestampTag > 0:
            string += struct.pack('Q', networkTime) # 8 bytes
        string += chr(computeCRC_8(string))
        self._bytes = string
        return string

    def bytes(self):
        return self._bytes
    def __repr__(self):
        if self.hasTime():
            return 'From:%5d To:%5d Type:%3d(%c) Payload:%s Included timing:%6d' % (self.fromId(), self.toId(), ord(self.ftype()), self.ftype(), self.payload(), self.networkTime())
        else:
            return 'From:%5d To:%5d Type:%3d(%c) Payload:%s' % (self.fromId(), self.toId(), ord(self.ftype()), self.ftype(), self.payload())
    

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

        frame = Frame()
        if not frame.fromReceived(rxbytes.bytes, int(rxbytes.timing-5000000.0*self.get_byte_send_time()), rxbytes.rssi):
            return None
        
        self.timeManager.frameReceived(frame)
        
        return frame

    def receiveFrame(self, deadline = None):
        while not deadline or time.time() < deadline:
            frame = self._receiveFrame(deadline)
            if frame:
                return frame
        return None

    def sendWholeFrame(self, frame, timing = None):
        if timing:
            #This might help combat the innacuracies of the get_approx_timing() method.
            frame['networkTime'] = timing + self.getNetworkTimeOffset()
            self.nic.timing(timing)
            return self.nic.tx(frame.toSend())
        else:
            return self.nic.tx(frame.toSend())
        
    def sendFrame(self, ftype, fromId, toId, payload, timing = None):
        frame = Frame({'ftype':ftype, 'fromId':fromId, 'toId':toId, 'payload':payload})
        return self.sendWholeFrame(frame, timing)

    # do not use it in protocols
    def _sync(self, deadline = None):
        self.nic.sync(deadline)
        
    # advanced
    def set_bitrate(self, bitrate):
        self.nic.set_bitrate(bitrate)
        
    # advanced
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
        
