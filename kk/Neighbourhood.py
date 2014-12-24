import sys, time, random
from .. import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
import Config

class Neighbourhood(Proto):
    frameTypes = 'n'
    neighbourhood = []
    TTL = 43

    def __init__(self):
        Proto.__init__(self)

    def handleFrame(self, frame):
        content = frame.content().translate(None, "[]()")
        content = content.split(",")
        tmp = []
        act = []
        i = 0
        for it in content:
            tmp.append(it)
            if i % 3 == 2:
                act.append((int(tmp[0]), int(tmp[1]), int(tmp[2])))
                tmp = []
            i += 1
        content = act
        content = map((lambda((a,b,c)): (a,b+1,c)), content)
        for it in content:
            match = False
            i = 0
            for el in self.neighbourhood:
                if el[0] == it[0]:
                    tmp = list(el) # remove next 3 lines if sth not working
                    tmp[1] = min(tmp[1], it[1]) 
                    self.neighbourhood[i] = tuple(tmp)
                    match = True
                i += 1
            if not match:
                self.neighbourhood.append(it)

    def send(self):
        print 'Sending frames...'
        if not self.neighbourhood:
            self.neighbourhood = [(self.frameLayer.getMyId(), 0, int(self.TTL + random.random() * 23.0))]
        self.frameLayer.sendFrame(ftype='n', fromId = self.frameLayer.getMyId(), toId=0, content=str(self.neighbourhood))
    
    def filterValid(self):
        self.neighbourhood = map((lambda((a,b,c)): (a,b,c-1)), self.neighbourhood)
        self.neighbourhood = filter((lambda((a,b,c)): c > 0.0), self.neighbourhood)
        def mem(a, b):
            for it in b:
                if it[0] == a:
                    return True
            return False
        if not mem(self.frameLayer.getMyId(), self.neighbourhood):
            self.neighbourhood.append((self.frameLayer.getMyId(), 0, int(self.TTL + random.random() * 15.0)))
        print 'Act neighbourhood for id %d:' % self.frameLayer.getMyId()
        print self.neighbourhood

    def onStart(self):
        initTime = time.time() + random.random() * 3.0
        interval = random.random() + 4.0
        self.dispatcher.scheduleRepeatingCallback(self.send, initTime, interval)
        self.dispatcher.scheduleRepeatingCallback(self.filterValid, initTime, 1.0)



