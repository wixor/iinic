#!/usr/bin/python

import sys, time, struct

from .. import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
from KeepAlive import KeepAlive
from PingPongProto import PingPongProto
from MonitorProto import MonitorProto
import Config

def sampleCallback():
    print 'This is sample callback, now is', time.time()

class SampleProto(Proto):
    frameTypes = 's'
    
    def __init__(self):
        Proto.__init__(self)
    
    def handleFrame(self, frame):
        print 'Sample proto handles a frame', frame
        
    def sampleCallback(self):
        print 'This is sample callback in sample proto, now is', time.time()
        
    def onStart(self):
        pass

def main(mode, interface):
    if interface == 'net':
        comm = iinic.NetComm()
    else:
        comm = iinic.USBComm(interface)
    #comm = iinic.USBComm() if Config.ON_DEVICE else iinic.NetComm()
    nic = iinic.NIC(comm)
    frameLayer = FrameLayer(nic)
    myId = frameLayer.getMyId()
    print >> sys.stderr, 'NIC initialized. My id is', frameLayer.getMyId()
    dispatcher = Dispatcher(frameLayer)
    if mode == 'k':
        keepalive = KeepAlive()
        dispatcher.registerProto(keepalive, 'keepalive')
        dispatcher.loop()

    if mode == 'd':
        sample = SampleProto()
        dispatcher.registerProto(sample, 'sample')
        
        pp = PingPongProto()
        dispatcher.registerProto(pp, 'ping-pong')
        
        try:
            dispatcher.registerProto(sample, 'sample')
        except:
            pass # yes, we expected you, Mr. Exception
        
        dispatcher.scheduleCallback(sample.sampleCallback, time.time()+1)
        dispatcher.scheduleCallback(sampleCallback, time.time()+2)
        dispatcher.scheduleRepeatingCallback(sampleCallback, time.time()+3, 10)
        dispatcher.loop()
    
    if mode == 'r':
        monitor = MonitorProto()
        dispatcher.registerProto(monitor, 'monitor')
        dispatcher.loop()

    if mode == 's':
        frameLayer.sendFrame('s', myId, 0, 'blah')
        approx = frameLayer.nic.get_approx_timing() # TODO: expose this method
        frameLayer.sendFrame('x', myId, 0, 'blah blah', approx + 2000000)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print 'Usage:', sys.argv[0], '[r|s|d|k]', '[net|USB interface]'
        sys.exit(1)
        
    main(sys.argv[1], sys.argv[2])
