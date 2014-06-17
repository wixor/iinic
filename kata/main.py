#!/usr/bin/python

import sys, time, struct

from .. import iinic
from ..kk.Frame import Frame, FrameLayer
from ..kk.Proto import Proto
from ..kk.Dispatcher import Dispatcher
from ..kk.PingPongProto import PingPongProto
from ..kk.MonitorProto import MonitorProto
from TimeSyncProto import TimeSyncProto
from SP import SP

import Config
from RoundProvider import RoundProvider

def main(device=None, send_interval=1000000, send_payload='blah'):
    comm = iinic.USBComm(device) if Config.ON_DEVICE else iinic.NetComm()
    nic = iinic.NIC(comm)
    frameLayer = FrameLayer(nic)
    myId = frameLayer.getMyId()
    print >> sys.stderr, 'NIC initialized. My id is', frameLayer.getMyId()
    ts = TimeSyncProto()
    rp = RoundProvider()
    sp = SP()
    dispatcher = Dispatcher(frameLayer, ts, rp)
    dispatcher.registerProto(sp, 'sp')
    
    dispatcher.loop()

if __name__ == '__main__':
    main()
