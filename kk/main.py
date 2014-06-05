#!/usr/bin/python

import sys, time, struct

from .. import iinic
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
from PingPongProto import PingPongProto
from MonitorProto import MonitorProto
from TimeSyncProto import TimeSyncProto
from SendingProto import SendingProto

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

def main(mode, device=None, send_interval=1000000, send_payload='blah'):
    comm = iinic.USBComm(device) if Config.ON_DEVICE else iinic.NetComm()
    nic = iinic.NIC(comm)
    frameLayer = FrameLayer(nic)
    myId = frameLayer.getMyId()
    print >> sys.stderr, 'NIC initialized. My id is', frameLayer.getMyId()
    dispatcher = Dispatcher(frameLayer)
    
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
    
    elif mode == 'r':
        monitor = MonitorProto()
        dispatcher.registerProto(monitor, 'monitor')
        dispatcher.loop()

    elif mode == 's':
        send = SendingProto(send_interval, send_payload)
        dispatcher.registerProto(send, 'send')
        dispatcher.loop()

    elif mode == 't':
        sync = TimeSyncProto()
        dispatcher.registerProto(sync, 'sync')
        dispatcher.loop()

    else:
        print 'Invalid mode', mode

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage:', sys.argv[0], '[r|s|d|t]'
        sys.exit(1)
        
    main(sys.argv[1])
