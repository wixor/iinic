import time, random

from Proto import Proto

class PingPongProto(Proto):
    INIT_RETRY = 2.0
    DROP_GAME = 4.0
    
    frameTypes = 'p'
    
    def __init__(self):
        Proto.__init__(self)
        self.lastReceived = None
        self.lastPing = None
        
    def onStart(self):
        self.dispatcher.scheduleCallback(self.initPing, time.time()+self.INIT_RETRY+1.0) # listen for some time
    
    def initPing(self):
        if not self.lastReceived or time.time()-self.lastReceived > self.DROP_GAME:
            timing = self.frameLayer.nic.get_approx_timing() + 1000000
            self.lastPing = self.timeManager.scheduleFrame(ftype='p', fromId=self.frameLayer.getMyId(), toId=0, payload='Ping 1', timing=timing)
            self.dispatcher.scheduleCallback(self.initPing, time.time() + self.INIT_RETRY + 0.69) # retry in some time
        
    def handleFrame(self, frame):
        if self.lastPing and not self.lastPing.acked:
            print 'Not sent previous message, ignore'
            return # ignore this frame
    
        if frame.toId() != 0 and frame.toId() != self.frameLayer.getMyId():
            return
        
        received = frame.content()
        try:
            nr = int(received[5:9])
        except ValueError:
            print 'Drop frame ', received
            return

        if received[0:4] == 'Ping':
            what = 'Pong'
        elif received[0:4] == 'Pong':
            what = 'Ping'
        else:
            return # drop frame
        
        print 'Received %s with timing %d, expected at %d' % (received, frame.recvTiming(), frame.timing() + self.timeManager.getNetworkTimeOffset())
        
        content = '%s %4d' % (what, nr+1)
        # schedule a reply one second after
        self.lastReceived = time.time()
        self.lastPing = self.timeManager.scheduleFrame(ftype='p', fromId=self.frameLayer.getMyId(), toId=frame.fromId(), payload=content, timing=frame.timing()+1000000)
        self.dispatcher.scheduleCallback(self.initPing, time.time()+self.DROP_GAME+0.1)
