import time, random

from ..kk.Proto import Proto
from ..kk.Frame import Frame

class SP(Proto):
    BEACON_INTERVAL = 1
    
    def __init__(self):
        Proto.__init__(self)
        self.frameTypes=''
    
    
    def onStart(self):
        self.dispatcher.timeManager.callOnSync(self.sendBeacon)

    def handleFrame(self, frame):
        pass
            
    def sendBeacon(self):
        frame = Frame({
            'ftype': 'x',
            'fromId': 0,
            'toId': 0,
            'payload': 'butt'})
        self.dispatcher.roundProvider.scheduleFrame(frame,self.dispatcher.roundProvider.getRoundNumber()+20)
