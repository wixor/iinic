import time, sys
from collections import defaultdict

from Frame import Frame, FrameLayer
from OurException import OurException

class Dispatcher:
    def __init__(self, frameLayer):
        self.typeToProto = defaultdict(lambda: [])
        self.nameToProto = {}
        self.callbacks = []
        self.frameLayer = frameLayer
        
    def registerProto(self, proto, name):
        if name in self.nameToProto:
            raise OurException('This protocol name has been already taken')
        
        # proto is valid
        for t in proto.frameTypes:
            self.typeToProto[t].append(proto)
        self.nameToProto[name] = proto
        
        proto.doRegistration(self)

    def getProtoByName(self, name):
        return self.nameToProto[name]

    def scheduleCallback(self, callback, timing):
        self.callbacks += [(callback, timing)]
        
    def scheduleRepeatingCallback(self, callback, firstCall, interval):
        def repeater():
            self.scheduleCallback(repeater, time.time() + interval)
            callback()
        self.scheduleCallback(repeater, firstCall)
        
    def loop(self):
        for (name, proto) in self.nameToProto.items():
            proto.onStart()

        while True:
            (callback, timing) = min(self.callbacks, key=lambda (c,t): t) if self.callbacks else (None, time.time()+10.)
            if timing <= time.time():
                if self.callbacks:
                    self.callbacks.remove( (callback,timing) )
                callback()
                # print 'Dispatching callback at', time.time(), 'scheduled', timing
            else:
                frame = self.frameLayer.receiveFrame(timing) # blocks until 'timing' has passed or a frame arrives
                if frame:
                    ftype = frame.type()
                    if ftype in self.typeToProto:
                        for proto in self.typeToProto[ftype]:
                            proto.handleFrame(frame)
                    else:
                        print >> sys.stderr, 'Cannot dispatch frame', frame
