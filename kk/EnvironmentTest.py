#!/usr/bin/python

import sys, time, struct
from Frame import Frame, FrameLayer
from Proto import Proto
from Dispatcher import Dispatcher
from EnvironmentProto import EnvironmentProto
from TimeManager import TimeManager
from .. import iinic
import Config

def main(mode):
    comm = iinic.USBComm() if Config.ON_DEVICE else iinic.NetComm()
    nic = iinic.NIC(comm)
    frameLayer = FrameLayer(nic)
    timeManager = TimeManager()
    print >> sys.stderr, 'NIC initialized. My id is', frameLayer.getMyId()
    dispatcher = Dispatcher(frameLayer, timeManager)
    
    frameType = 'b'
        
    if mode == '1':
        frameLayer.sendFrame(frameType, frameLayer.getMyId(), 0, '0')
   
    elif mode == '2':
        environmentProto = EnvironmentProto(frameType)
        dispatcher.registerProto(environmentProto, 'environmentProto')
        dispatcher.scheduleRepeatingCallback(environmentProto.printNeighbours, time.time()+2, 5)
        dispatcher.loop()

    else:
        print 'Invalid mode', mode

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage:', sys.argv[0], '[1|2]'
        sys.exit(1)
        
    main(sys.argv[1])
