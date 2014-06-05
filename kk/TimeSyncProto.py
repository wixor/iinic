import time, random

from Frame import Frame, FrameLayer
from Dispatcher import Dispatcher
from Proto import Proto
import Config

class State:
    PREPARING = 0
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
    
    def _approxRoundDuration(self): 
        if 'roundDuration' not in self.__dict__:
            self.roundDuration = (255.0 + 10.0 + Frame.lengthOverhead()) * self.frameLayer.get_byte_send_time()
        # print 'round duration: ', self.roundDuration
        return self.roundDuration
    
    def _computeBackoff(self, index):
        return 2.0+index*index # TODO: be more smart

    def _isLess(self, x, y):
        return x + 1000000.0 * self._approxRoundDuration() / 275.0 < y
    
    def _sendSyncFrame(self):
        frame = Frame()
        timing = self.getApproxTiming() + 100000
        self.frameLayer.sendFrame(ftype='s', fromId=self.frameLayer.getMyId(), toId=0, content=str(timing+self.clockDiff), timing=timing)

    def _changeState(self, newState):
        self.state = newState
        self.stateNo += 1

    ## logic functions ##

    def _gotSynced(self):
        self._masterSync(self.stateNo, 1)
        print 'Got synced, time diff %d' % (self.clockDiff)
        # TODO: notify protocols
        
    def _lostSync(self):
        pass
        # TODO: notify protocols

    def _startBeingMaster(self):
        if self.state != State.STARTING:
            return # too late
        
        self._changeState(State.SYNCED)
        self.clockDiff = 0 # clock difference is zero :)
        self._gotSynced()

    def _mustSync(self, stateNo, callNo): # TODO: there will be conflicts!
        if stateNo != self.stateNo or callNo > self.MUST_SYNC_REPEAT:
            return
    
        if random.randint(1, 2**callNo) == 1:
            self._sendSyncFrame()

        backoff = self._computeBackoff(callNo)
        self.dispatcher.scheduleCallback(lambda: self._mustSync(stateNo, callNo+1), time.time() + backoff*self._approxRoundDuration())

    def _demandSync(self, stateNo, callNo):
        if stateNo != self.stateNo:
            return

        self._sendSyncFrame()
        backoff = self._computeBackoff(callNo)
        self.dispatcher.scheduleCallback(lambda: self._demandSync(stateNo, callNo+1), time.time() + backoff*self._approxRoundDuration())

    def _masterSync(self, stateNo, callNo):
        if stateNo != self.stateNo or callNo > self.MASTER_SYNC_FRAMES_COUNT:
            return

        self._sendSyncFrame()
        backoff = self._computeBackoff(callNo)
        self.dispatcher.scheduleCallback(lambda: self._masterSync(stateNo, callNo+1), time.time() + backoff*self._approxRoundDuration())
        
    def _debugCallback(self):
        log('State %d, stateNo %d, round %1.6f, clockDiff %d' % (self.state, self.stateNo, self._approxRoundDuration(), self.clockDiff))

    ## public functions ##

    def __init__(self):
        Proto.__init__(self)
        if not self.frameTypes:
            self.frameTypes = ''
            for i in xrange(0,256):
                self.frameTypes += chr(i)
        self.clockDiff = 0
        self.stateNo = 0

    def onStart(self):
        self._changeState(State.PREPARING)
        self.syncWithCard()
        self.dispatcher.scheduleCallback(self.startAlgo, time.time() + 1.5)
    
    def startAlgo(self):
        self._changeState(State.STARTING)
        self.dispatcher.scheduleCallback(self._startBeingMaster, time.time()+self.LISTEN_ON_START*self._approxRoundDuration())
        self.dispatcher.scheduleRepeatingCallback(self._debugCallback, time.time()+0.1, 1.0)
        
    def handleFrame(self, frame):
        if self.state == State.PREPARING:
            return
        
        ftype = frame.type()
        recvTime = None
        if ftype == 's':
            try:
                recvTime = int(frame.content())
            except ValueError:
                recvTime = None
    
        if recvTime: # is sync frame!
            myTime = frame.timing() + self.clockDiff
            print 'received sync frame at %d (%d) with %d' % (frame.timing(), myTime, recvTime)
            if self.state == State.SYNCED:
                if self._isLess(recvTime, myTime): # somebody has lower time
                    log('Must sync somebody, times: %d vs %d (%d)' % (myTime, recvTime, myTime-recvTime))
                    self.dispatcher.scheduleCallback(lambda: self._mustSync(self.stateNo, 1), time.time()+random.random()*5*self._approxRoundDuration())
                elif self._isLess(myTime, recvTime): # somebody has higher time
                    log('Lost sync to somebody, times: %d vs %d (%d)' % (myTime, recvTime, myTime-recvTime))
                    self.clockDiff = 0
                    self._changeState(State.DEMAND_SYNC)
                    self._lostSync()
                    #TODO: notify other protocols that we lost sync
                else :
                    log('Frame ok.')

            if self.state != State.SYNCED:
                if self._isLess(recvTime, myTime):
                    log('Ignore as for now')
                    pass # ignore
                else:
                    self.clockDiff = recvTime - frame.timing()
                    self._changeState(State.SYNCED)
                    self._gotSynced()
        
        else: # some other frame
            if self.state == State.STARTING:
                self._changeState(State.DEMAND_SYNC)
                self.dsNo = 1
                self._demandSync(self.stateNo, 1)
                
    
    def syncWithCard(self):
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
        print 'Card time diff:', self.approxCardTimeDiff
        
    def getApproxTiming(self):
        return self.frameLayer.nic.get_approx_timing() + self.approxCardTimeDiff
        
        
        