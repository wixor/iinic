#!/usr/bin/python

import sys, time, struct

from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
from PingPongProto import PingPongProto
import Config

def sampleCallback():
    print 'This is sample callback, now is', time.time()

class SampleProto(Proto):
    def __init__(self, frameLayer):
        Proto.__init__(self, frameLayer)
    
    def handleFrame(self, frame):
        print 'Sample proto handles a frame', frame
        
    def sampleCallback(self):
        print 'This is sample callback in sample proto, now is', time.time()

def main(mode):
    frameLayer = FrameLayer()
    frameLayer.nic.set_channel(23) # TODO: expose this method
    print >> sys.stderr, 'NIC initialized. My id is', frameLayer.getMyId()
    
    if mode == 'd':
        dispatcher = Dispatcher(frameLayer)
        
        sample = SampleProto(frameLayer)
        dispatcher.registerProto(sample, 'sample', 's')
        
        pp = PingPongProto(frameLayer)
        dispatcher.registerProto(pp, 'ping-pong', 'p')
        
        try:
            dispatcher.registerProto(sample, 'foo', 's')
        except:
            pass # yes, we expected you, Mr. Exception
        
        dispatcher.scheduleCallback(sample.sampleCallback, time.time()+1)
        dispatcher.scheduleCallback(sampleCallback, time.time()+2)
        dispatcher.scheduleRepeatingCallback(sampleCallback, time.time()+3, 10)
        dispatcher.loop()
    
    if mode == 'r':
        while True:
            frame = frameLayer.receiveFrame(deadline = time.time() + 100.0)
            if frame:
                print 'Timing:', frame.timing(), frame

    if mode == 's':
        frameLayer.sendFrame('s', myId, 0, 'blah')
        approx = frameLayer.nic.get_approx_timing() # TODO: expose this method
        frameLayer.sendFrame('x', myId, 0, 'blah blah', approx + 2000000)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage:', sys.argv[0], '[r|s]'
        sys.exit(1)
        
    main(sys.argv[1])
