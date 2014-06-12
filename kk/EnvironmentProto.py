import time, random

from Proto import Proto

class EnvironmentProto(Proto):
    TIMEOUT = 15
    BEACON_INTERVAL = 5
    
    def __init__(self, frameType):
        Proto.__init__(self)
        self.__neighbours = []
        self.frameTypes = frameType

    def onStart(self):
        self.dispatcher.scheduleRepeatingCallback(self.sendBeacon, time.time(), self.BEACON_INTERVAL)

    def handleFrame(self, frame):
        try:
            fromId = frame.fromId()
            power = 100 # frame.power() doesn't work 
            content = frame.content()
            neighboursNumber = int(frame.content())
        except: 
            # incorrect frame - ignore it
            return
            
        result = [n for n in self.__neighbours if n[0] != fromId]
        result += [(fromId, time.time(), power, neighboursNumber)]
        self.__neighbours = result
            
    def sendBeacon(self):
        timing = self.frameLayer.nic.get_approx_timing() + 1000000
        # add random time - maximally 1/4 of BEACON_INTERVAL
        timing = timing + random.randint(0, 250000 * self.BEACON_INTERVAL)
        print '~ Sending beacon.. ', timing, '(hardware time) ~'
        # content of a frame is a number of my neighbours
        content = str(len(self.getNeighbours()))
        self.frameLayer.sendFrame(self.frameTypes, self.frameLayer.getMyId(), 0, content, timing)
    
        
    def getNeighbours(self):
        self.removeOldNeighbours()
        return self.__neighbours
        
    def printNeighbours(self):
        print '------------------------------------------'
        print 'Current time: ', time.time(), '(pc time)'
        print 'Neighbours list:'
        for n in self.getNeighbours():
            print n
        print '------------------------------------------'

    def removeOldNeighbours(self):
        lowerBound = time.time() - self.TIMEOUT
        result = [n for n in self.__neighbours if n[1] >= lowerBound]
        self.__neighbours = result
        
