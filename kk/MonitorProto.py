from Proto import Proto
from OurException import OurException

class MonitorProto(Proto):
    frameTypes = ''
    def __init__(self):
        Proto.__init__(self)
        for i in xrange(1, 255):
            self.frameTypes += chr(i)
        
    def onStart(self):
        pass
        
    def handleFrame(self, frame):
        print 'Timing:', frame.timing(), frame