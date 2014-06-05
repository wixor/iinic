import time, random

from Proto import Proto

class EnvironmentProto(Proto):
    TIMEOUT = 30
    BEACON_INTERVAL = 10
    
    def __init__(self, frameType):
        Proto.__init__(self)
        self.__neighbours = []
        self.frameTypes = frameType

    def onStart(self):
        self.dispatcher.scheduleRepeatingCallback(self.sendBeacon, time.time(), self.BEACON_INTERVAL)
        self.dispatcher.scheduleRepeatingCallback(self.removeOldNeighbours, time.time()+1, 3)

    def handleFrame(self, frame):
        result = [n for n in self.__neighbours if n[0] != frame.fromId()]
        power = 100 #frame.power() doesn't work 
        result += [(frame.fromId(), time.time(), power)]
        self.__neighbours = result
        
    def sendBeacon(self):
        self.frameLayer.sendFrame(self.frameTypes, self.frameLayer.getMyId(), 0, '', 1)
        
    def getNeighbours(self):
        return self.__neighbours
        
    def printNeighbours(self):
        print 'Current time: ', time.time()
        print 'Neighbours list:'
        for n in self.getNeighbours():
            print n
    
    def removeOldNeighbours(self):
        lowerBound = time.time() - self.TIMEOUT
        result = [n for n in self.__neighbours if n[1] >= lowerBound]
        self.__neighbours = result
        
