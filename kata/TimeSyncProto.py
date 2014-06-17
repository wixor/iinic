import time, random

from ..kk.Frame import Frame, FrameLayer
from ..kk.Dispatcher import Dispatcher
from ..kk.Proto import Proto
import Config
import random

class State:
    PREPARING = -1
    WAITING_FOR_TRAFFIC_ANY = 0
    WAITING_FOR_TRAFFIC_EXTENDED = 1
    WAITING_FOR_RESPONSE = 2
    SYNCED = 3

def log(s):
    t = time.time()
    ti = int(t)
    t -= 100.0*(ti/100)
    print '%02.6f: %s' % (t, s)

class TimeSyncProto(Proto):
    LISTEN_FOR_TRAFFIC = 10
    LISTEN_FOR_STAMPED_TRAFFIC = 20
    LISTEN_FOR_RESPONSE = 15
    MAX_RETRIES = 2
    RETRIES = 0
    
    frameTypes = ''
    roundProtoName = None # TODO: communicate with round protocol

    heardTraffic = False
    offsetFromLocalTime = 0
    
    ## helper functions ##

    def _localToNetwork(self, timing):
        return timing + self.offsetFromLocalTime
    
    def _approxRoundDuration(self): 
        if 'roundDuration' not in self.__dict__:
            self.roundDuration = (255.0 + 10.0 + Frame.lengthOverhead()) * self.frameLayer.get_byte_send_time()
        return self.roundDuration
    
    def _computeBackoff(self, index):
        return 2**index

    def getApproxNow(self):
        try:
            diff = self.approxCardTimeDiff
        except:
            diff = 700000
        return self.frameLayer.nic.get_approx_timing() + diff + 1000

    def _sendSyncRequest(self):
        frame = Frame()
        timing = self.getApproxNow() + 100000
        log('Sending sync req frame at local time %d (network %d)' % (timing, self._localToNetwork(timing)))
        self.frameLayer.sendFrame(ftype='s', fromId=self.frameLayer.getMyId(), toId=0, payload='req', timing=timing)

    def _sendSyncResponse(self):
        #TODO: We'd like to send this in an appropriate round
        frame = Frame()
        timing = self.getApproxNow() + 100000*random.uniform(1, 4)
        log('Sending sync resp frame at local time %d (network %d)' % (timing, self._localToNetwork(timing)))
        self.frameLayer.sendFrame(ftype='s', fromId=self.frameLayer.getMyId(), toId=0, payload='resp', timing=timing)

    def _changeState(self, newState):
        self.state = newState
        if newState == State.SYNCED:
            for callback in self.callOnSyncList:
                callback()
            self.callOnSyncList = []

    def _progressState(self):
        if self.state==State.PREPARING:
            log('Started listening for traffic')
            self._changeState(State.WAITING_FOR_TRAFFIC_ANY)
            self.dispatcher.scheduleCallback(self._progressState, time.time()+self.LISTEN_FOR_TRAFFIC*self._approxRoundDuration())
            return
        if self.state==State.WAITING_FOR_TRAFFIC_ANY:
            if self.heardTraffic:
                log('Listening for traffic a ltittle longer')
                self._changeState(State.WAITING_FOR_TRAFFIC_EXTENDED)
                self.dispatcher.scheduleCallback(self._progressState, time.time()+self.LISTEN_FOR_STAMPED_TRAFFIC*self._approxRoundDuration())
            else:
                log('Sending a sync frame and waiting for a response')
                self._sendSyncRequest()
                self._changeState(State.WAITING_FOR_RESPONSE)
                self.dispatcher.scheduleCallback(self._progressState, time.time()+self.LISTEN_FOR_RESPONSE*self._approxRoundDuration())
            return
        if self.state==State.WAITING_FOR_TRAFFIC_EXTENDED:
            log('Sending a sync frame and waiting for a response')
            self._sendSyncRequest()
            self._changeState(State.WAITING_FOR_RESPONSE)
            self.dispatcher.scheduleCallback(self._progressState, time.time()+self.LISTEN_FOR_RESPONSE*self._approxRoundDuration())
            return
        if self.state==State.WAITING_FOR_RESPONSE:
            if self.RETRIES < self.MAX_RETRIES:
                log('Retrying time request')
                self._sendSyncRequest()
                self.dispatcher.scheduleCallback(self._progressState, time.time()+self.LISTEN_FOR_RESPONSE*self._approxRoundDuration()*self._computeBackoff(self.RETRIES))
                self.RETRIES+=1
                return
            log('synced')
            self._changeState(State.SYNCED)
        if self.state==State.SYNCED:
            pass
        
    def _debugCallback(self):
        pass
        log('State %d, offset %d' % (self.state, self.offsetFromLocalTime))

    ## public functions ##

    def __init__(self):
        Proto.__init__(self)
        self.offsetFromLocalTime = 0
        self.approxCardTimeDiff = 100000 if Config.ON_DEVICE else 700000
        self.callOnSyncList = []

    def callOnSync(self, callback):
        self.callOnSyncList.append(callback)

    def onStart(self):
        self._changeState(State.PREPARING)
        self.dispatcher.scheduleCallback(self.startAlgo, time.time() + 1.5)
    
    def startAlgo(self):
        self.dispatcher.scheduleRepeatingCallback(self._debugCallback, time.time()+0.1, 5.0)
        self._progressState()

    def handleFrame(self, frame):
        pass
    
    def getNetworkTimeOffset(self):
        return self.offsetFromLocalTime

    def synced(self):
        return self.state == State.SYNCED

    def frameReceived(self, frame):
        self.heardTraffic = True #We're not alone on the network
        
        if self.state == State.PREPARING:
            return
        
        if not frame.hasTime():
            return

        recvTime = self._localToNetwork(frame.timing())
        sendTime = frame['networkTime']

        log('local time: %d, remote time: %d' % (recvTime, sendTime))
        
        if recvTime > sendTime and self.state == State.SYNCED:
            #We have a message from a younger node and we can correct it authoritatively
            log('Timing recieved from younger node (%d by %d)' % (frame['fromId'], recvTime - sendTime))
            self._sendSyncResponse()
            return
        if recvTime < sendTime and not (frame['ftype'] == 's' and frame['payload'] == 'req'):
            #We got a packet from an older node that isn't asking for the time
            delta = sendTime - recvTime
            log('Timing recieved from older node %d, updating by %d' % (frame['fromId'], delta))
            self.offsetFromLocalTime += delta
            self._changeState(State.SYNCED)
