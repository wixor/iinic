import time

from Proto import Proto

class PingPongProto(Proto):
    INIT_RETRY = 2.0
    DROP_GAME = 4.0
    
    def __init__(self):
        Proto.__init__(self)
        self.lastReceived = None
        
    def onStart(self):
        self.dispatcher.scheduleCallback(self.initPing, time.time()+self.INIT_RETRY+0.1) # listen for some time
    
    def initPing(self):
        if not self.lastReceived or time.time()-self.lastReceived > self.DROP_GAME:
            self.frameLayer.sendFrame(ftype='p', fromId=self.frameLayer.getMyId(), toId=0, content='Ping 1')
            self.dispatcher.scheduleCallback(self.initPing, time.time() + self.INIT_RETRY) # retry in 2 seconds
        
    def handleFrame(self, frame):
        if frame.fromId() == self.frameLayer.getMyId():
            return
        
        self.lastReceived = time.time()
        received = frame.content()
        if received[0:4] == 'Ping':
            reply = 'Pong'
        elif received[0:4] == 'Pong':
            reply = 'Ping'
        else:
            return # drop frame
        
        try:
            reply += ' ' + str(1 + int(received[5:]))
        except ValueError:
            return # drop frame
        
        # reply immediately
        self.frameLayer.sendFrame(ftype='p', fromId=self.frameLayer.getMyId(), toId=frame.fromId(), content=reply, timing=frame.timing()+1000000)
        self.dispatcher.scheduleCallback(self.initPing, time.time()+self.DROP_GAME+0.1)
