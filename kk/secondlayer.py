import sys, time, struct

from .. import iinic
from firstlayer import FirstLayer, Frame, FrameType


class SecondLayer:
    def __init__(self, nic, myId):
        self.myId = myId
        self.nic = nic
        self.firstLayer = FirstLayer(nic, myId)
        self.constructTime = time.time()
        
        self.isMaster = False
        self.isSync = False
        self.clockDiffToMaster = None
        self.roundStartTime = None # in master's time!
        
    def _roundTime(self):
        return 1000000 # 1000000/self.nic._txbitrate*8*(256 + 12)
    
    def _approxCardTime(self):
        return (time.time()-self.constructTime) * 1000000
    
    def _toRoundNumber(self, cardTime):
        raise Exception('Not implemented')
    
    def _sendSyncFrame(self, timing, toId = 0): # if toId != 0, send a personalized frame
        # pack following values: my clock (=timing), some round start time
        data = struct.pack('qq', timing, self.roundStartTime)
        self.firstLayer.sendFrame(FrameType.SYNC, self.myId, toId, data, timing)
        
    def _synchronize(self): # synchronize time with neighbourhood
        # - listen to communication for a while
        #   - if nothing heard => I am the master and start sending "sync" frames
        #     - however, if a sync frame received with (much) higher time, make this guy my master -> TODO
        #   - if a "sync" frame heard => choose this guy as my master
        # - send my future master a slave request (TODO: when and how?)
        # - wait for him to reply with a personalized sync
        someTime = time.time() + 3.
        wannabeMaster = None
        allNeighs = dict()
        # listen ...
        while time.time() < someTime:
            frame = self.firstLayer.receiveFrame(deadline = someTime)
            if not frame:
                continue
            
            if frame.type() == 'S' and frame.toId() == 0:
                wannabeMaster = frame.fromId()
                break
                
            allNeighs[frame.fromId()] = frame.power() if frame.power() else 1

        # if haven't got sync frame, select a guy with highest power
        if not wannabeMaster and allNeighs:
            wannabeMaster = max(allNeighs.items(), key=lambda (k,v): v)[0]
            
        if not wannabeMaster:
            # I am the master, hahaha!
            self.isSync = True
            self.isMaster = True
            self.roundStartTime = (time.time()-self.constructTime+1) * 1000000
            self.clockDiffToMaster = 0
            return True

        # try 10 times, then fail
        for i in xrange(10):
            currRound = self._toRoundNumber(self._approxCardTime())
            self.firstLayer.sendFrame(
                ftype = FrameType.SLAVE_REQUEST,
                fromId = self.myId,
                toId = wannabeMaster, 
                data = '',
                timing = 0) # now
            deadline = time.time() + 1.
            while time.time() < deadline:
                frame = self.firstLayer.receiveFrame(deadline = deadline)
                if frame and frame.type() == FrameType.SYNC and frame.fromId() == wannabeMaster and frame.toId() == self.myId:
                    self.isSync = True
                    self.isMaster = False
                    content = frame.content()
                    (t, s) = struct.unpack('qq', content)
                    self.clockDiffToMaster = t - frame.timing() # may be negative, something bad?
                    self.roundStartTime = s
                    return True
                
        return False
        
    def doSync(self):
        if self._synchronize():
            print >> sys.stderr, 'Synchronized! Round starts at', self.roundStartTime, '+ every', self._roundTime()
            if self.isMaster:
                raise Exception('Not implemented sending \'sync\' frames')
            else:
                print >> sys.stderr, 'Could not synchronize.'