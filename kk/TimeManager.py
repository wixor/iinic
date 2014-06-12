import time, random, sys

from Frame import Frame, FrameLayer
from Dispatcher import Dispatcher
from Proto import Proto
import Config

def log(s):
    t = time.time()
    ti = int(t)
    t -= 100.0*(ti/100)
    print '%02.6f: %s' % (t, s)
    
class TimeManager(Proto):
    def __init__(self):
        self.frameTypes = ''
        self.approxCardTimeDiff = 100000 if Config.ON_DEVICE else 700000
        
    def handleFrame(self, frame):
        # it doesn't handle frames
        pass
    
    def onStart(self):
        self._syncWithCard()
        pass
    
    def getApproxNow(self):
        try:
            diff = self.approxCardTimeDiff
        except:
            diff = 700000
        return self.frameLayer.nic.get_approx_timing() + diff + 1000
    
    def scheduleFrame(self, ftype, fromId, toId, payload, timing = None): # send in first available slot after 'timing'
        diff = self.getNetworkTimeOffset()
        if timing is None:
            timing = self.getApproxNow() # do not send in the past!
        else:
            try:
                timing += self.roundTripTime
            except AttributeError:
                log('Warning, no roundTripTime.')
                timing += 100000

        if diff is None: # not synced or other error, do not care about rounds.
            sendTiming = timing
        else:
            roundTime = self._getRoundDuration()
            sendTiming = roundTime * (int((timing+diff) / roundTime) + 1) - diff + self._getRoundOffset()
            
        self._scheduleFrame(ftype, fromId, toId, payload, sendTiming)

    def frameReceived(self, frame):
        if frame.recvTiming() is None:
            return # nothing to do
        recvNetworkOffset = frame.recvTiming() - frame.timing()
        myNetworkOffset = self.getNetworkTimeOffset()
        if recvNetworkOffset > (myNetworkOffset or 0) + 10: # allow some tolerance
            log('Changing offset, old %d, new %d' % (myNetworkOffset or 0, recvNetworkOffset))
            self.clockDiff = recvNetworkOffset

    def getNetworkTimeOffset(self):
        if 'clockDiff' not in self.__dict__:
            return None
        return self.clockDiff

    def _getRoundDuration(self):
        if 'roundDuration' not in self.__dict__:
            self.roundDuration = (255.0 + Frame.lengthOverhead() + Config.DEVICE_BYTES_SILENCE_BEFORE + Config.DEVICE_BYTES_SILENCE_AFTER) * self.frameLayer.get_byte_send_time() * 1000000
            pow10 = 1
            while self.roundDuration > pow10*10:
                pow10 *= 10
            self.roundDuration = int(self.roundDuration / pow10 + 1) * pow10
            log(sys.stderr,'Round duration %d' % (self.roundDuration))
        return self.roundDuration

    def _getRoundOffset(self, shiftBytes = 0):
        if 'roundOffset' not in self.__dict__:
            self.roundOffset = (shiftBytes + Config.DEVICE_BYTES_SILENCE_BEFORE) * self.frameLayer.get_byte_send_time() * 1000000
        return self.roundOffset
    
    def _scheduleFrame(self, ftype, fromId, toId, payload, timing):
        self.frameLayer.sendFrame(ftype, fromId, toId, payload, timing)
        
    ## sync with card ====================================================================== ##
        
    def _syncWithCard(self):
        self._roundTripSent = time.time()
        pingFuture = self.frameLayer.nic.ping()
        pingFuture.add_callback(self._ping1back)
        
        approx = self.frameLayer.nic.get_approx_timing()
        self._syncPingSent = approx+1000000
        self.frameLayer.nic.timing(self._syncPingSent) # 1 sec should be safe
        pingFuture = self.frameLayer.nic.ping()
        pingFuture.add_callback(self._ping2back)

    def _ping1back(self):
        self.roundTripTime = int(1000000*(time.time() - self._roundTripSent))

    def _ping2back(self):
        if 'roundTripTime' not in self.__dict__:
            raise OurException('Sync with card failed.')
        self.approxCardTimeDiff = self._syncPingSent - self.frameLayer.nic.get_approx_timing() + self.roundTripTime
        log(sys.stderr, 'Ready. Roundtrip time %d, time diff between approx_time() and real time %d' % (self.roundTripTime, self.approxCardTimeDiff))
            