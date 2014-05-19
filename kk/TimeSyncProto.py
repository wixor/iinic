import time, random

from Frame import Frame, FrameLayer
from Dispatcher import Dispatcher
from Proto import Proto
import Config

class State:
    STARTING = 1
    DEMAND_SYNC = 2 # heard something, waiting for sync
    SYNCED = 3

# sync frame has: timing (6 bytes)

# 1. wait for some time and listen
# 2. if nothing heard: I am the master and send a few sync messages after X, 2X, 4X, 8X, ... seconds
# 3. if something heard: send sync frames repeatedly until somebody hears me (which may never happen btw)
# 4. if a sync frame heard with:
#  * lower timing: ignore it temporarily until I am synced
#  * higher timing: I am sync'd and finish, send them a frame confirming sync every time they want me to sync
#  * 
# 5. if I am synced:
# if received with lower timing, sync them until I receive a message

def log(s):
    t = time.time()
    ti = int(t)
    t -= 100.0*(ti/100)
    print '%02.6f: %s' % (t, s)

class TimeSyncProto(Proto):
    LISTEN_ON_START = 10 # listen for 20X rounds
    MASTER_SYNC_FRAMES_COUNT = 5 # send X sync frames after decided I am the master (1 round, 2 rounds, 4, 8, ...)
    MUST_SYNC_REPEAT = 5 # send X sync frames, each with prob 2**(-i)
    frameTypes = ''
    roundProtoName = None # TODO: communicate with round protocol
    
    ## helper functions ##
    
    def _roundDuration(self):
        if 'roundDuration' not in self.__dict__:
            self.roundDuration = (255.0 + Config.SILENCE_BEFORE + Config.SILENCE_AFTER + Frame.lengthOverhead()) * self.frameLayer.get_byte_send_time()
        return self.roundDuration
    
    def _computeBackoff(self, index):
        return 5.0*(1+index*index) # TODO: be more smart
    
    def _isLess(self, x, y):
        return x + 1000000.0 * self._roundDuration() * 0.01 < y
    
    def _sendSyncFrame(self):
        frame = Frame()
        # def sendFrame(self, ftype, fromId, toId, content, timing = None):
        timing = self.frameLayer.nic.get_approx_timing() + 100
        log('  approx timing %d' % (timing,))
        self.frameLayer.sendFrame(ftype='s', fromId=self.frameLayer.getMyId(), toId=0, content=str(timing), timing=timing)

    ## logic functions ##
    
    def _gotSynced(self):
        log('Got synced, time diff %d' % (self.clockDiff))
        pass # TODO: do something!
    
    def _startBeingMaster(self):
        if self.state != State.STARTING:
            return # nope.
        else:
            self.state = State.SYNCED
            self.clockDiff = 0 # clock difference is zero :)
            self._masterSync()
            self._gotSynced()
            
    def _mustSync(self): # TODO: nope, there will be conflicts!
        if random.randint(1, 2**self.msNo) == 1:
            self._sendSyncFrame()
            
        if self.msNo < self.MUST_SYNC_REPEAT:
            backoff = self._computeBackoff(self.msNo)
            self.msNo += 1
            self.dispatcher.scheduleCallback(self._mustSync, backoff*self._roundDuration())
        else:
            self.msNo = 1
            
    def _demandSync(self):
        if self.state | State.SYNCED:
            return # nothing to demand
        self._sendSyncFrame()
        backoff = self._computeBackoff(self.bsNo)
        self.bsNo += 1
        self.dispatcher.scheduleCallback(self._demandSync, backoff*self._roundDuration())
    
    def _masterSync(self):
        self._sendSyncFrame()
        backoff = self._computeBackoff(self.masterNo)
        if self.masterNo == self.MASTER_SYNC_FRAMES_COUNT:
            self.masterNo = 1
        else:
            self.masterNo += 1
            self.dispatcher.scheduleCallback(self._masterSync, backoff*self._roundDuration())
        
    def _debugCallback(self):
        log('State %d, msNo %d, dsNo %d, round %1.6f, clockDiff %d' % (self.state, self.msNo, self.dsNo, self._roundDuration(), self.clockDiff))
    
    ## public functions ##
    
    def __init__(self):
        Proto.__init__(self)
        if not self.frameTypes:
            self.frameTypes = ''
            for i in xrange(0,256):
                self.frameTypes += chr(i)
        self.msNo = 1
        self.dsNo = 1
        self.masterNo = 1
        self.clockDiff = 0

    def onStart(self):
        self.state = State.STARTING
        self.dispatcher.scheduleCallback(self._startBeingMaster, time.time()+self.LISTEN_ON_START*self._roundDuration())
        self.dispatcher.scheduleRepeatingCallback(self._debugCallback, time.time()+0.1, 1.0)
        
    def handleFrame(self, frame):
        ftype = frame.type()
        recvTime = None
        if ftype == 's':
            try:
                recvTime = int(frame.content())
            except ValueError:
                recvTime = None
    
        if recvTime: # is sync frame!
            myTime = frame.timing() + self.clockDiff
            if self.state == State.SYNCED:
                if self._isLess(recvTime, myTime): # somebody has lower time
                    log('Must sync somebody, times: %d vs %d' % (myTime, recvTime))
                    self.dispatcher.scheduleCallback(self._mustSync, time.time()+random.random()*5.0)
                else:
                    log('Lost sync to somebody, times: %d vs %d' % (myTime, recvTime))
                    self.clockDiff = 0
                    self.state = State.DEMAND_SYNC
                    self._demandSync()
                    #TODO: notify others that we lost sync
                    
            if self.state != State.SYNCED:
                if self._isLess(recvTime, myTime):
                    log('Ignore as for now')
                    pass # ignore
                else:
                    self.clockDiff = recvTime - myTime
                    self.state = State.SYNCED
                    self._gotSynced()
        
        else: # some other frame
            if self.state == State.STARTING:
                self.state = DEMAND_SYNC
                self._demandSync()