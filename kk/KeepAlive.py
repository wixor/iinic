# badziewne ramki na zasadzie keepalive, ale zapewniaja regularny dostep do
#  - info o sasiadach
#  - ich timingu
import sys, time, random
from .. import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
import Config

class KeepAlive(Proto):
    frameTypes = 'k'
    neighbours = []
    TTL = 15.0

    def __init__(self):
        Proto.__init__(self)

    def handleFrame(self, frame):
        frameData = (frame.fromId(), frame.timing())
        print 'KEEPALIVE from ID = %d at time = %d' % frameData
        def member(a,b):
            for i in range(len(b)):
                if a == b[i][0]:
                    return i
            return -1

        i = member(frame.fromId(),self.neighbours)
        if i > -1:
            self.neighbours[i] = (frame.fromId(), self.TTL)
        else:
            self.neighbours.append( (frame.fromId(), self.TTL) )

    def send(self):
        print 'Sending a keepalive...'
        self.frameLayer.sendFrame(ftype='k', fromId = self.frameLayer.getMyId(), toId=0, content='')
    
    def freezeNeighbours(self):
        self.neighbours = map((lambda((a,b)): (a,b-1)), self.neighbours)
        self.neighbours = filter((lambda((a,b)): b > 0.0), self.neighbours)
        print '--TTL + removing old ones'
        print self.neighbours

    def onStart(self):
        initTime = time.time() + random.random() * 3.0
        self.dispatcher.scheduleRepeatingCallback(self.send, initTime, random.random() + self.TTL / 3.0)
        self.dispatcher.scheduleRepeatingCallback(self.freezeNeighbours, initTime, 1.0)



