# badziewne ramki na zasadzie keepalive, ale zapewniaja regularny dostep do
#  - info o sasiadach
#  - ich timingu
import sys, time
sys.path.append('/home/bjornwolf/iinic/kk')
sys.path.append('/home/bjornwolf/iinic')
import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
import Config

class KeepAlive(Proto):
    frameTypes = 'k'

    def __init__(self):
        Proto.__init__(self)

    def handleFrame(self, frame):
        frameData = (frame.fromId(), frame.timing(), frame.power())
        print 'KEEPALIVE from ID = %d at time = %d and power = %d' % frameData

    def send(self):
        print 'Sending a keepalive...'
        self.frameLayer.sendFrame(ftype='k', fromId = self.frameLayer.getMyId(), toId=0, content='')
    
    def onStart(self):
        initTime = time.time() + random.random() * 3.0
        self.dispatcher.scheduleRepeatingCallback(self.send, initTime, 5.0)


