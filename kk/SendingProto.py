import time

from .. import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher

class SendingProto(Proto):
    frameTypes = ''
    def __init__(self, i, pay):
        self.interval = i
        self.cnt = 0
        self.payload = pay
        
    def sendFrame(self):
        payload = '%s %d' % (self.payload, self.cnt)
        self.frameLayer.sendFrame('x', self.myId, 0, payload, timing = None)
        self.cnt += 1
        self.dispatcher.scheduleCallback(self.sendFrame, time.time() + 1.0*self.interval/1000000)
        
    def onStart(self):
        self.myId = self.frameLayer.getMyId()
        self.sendFrame()