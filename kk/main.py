#!/usr/bin/python

import sys, time

from .. import iinic
from firstlayer import FirstLayer
from secondlayer import SecondLayer

def main(mode):
    nic = iinic.NIC(iinic.NetComm())
    print >> sys.stderr, 'NIC initialized'
    
    if mode == 'r':
        layer = FirstLayer(nic, myId = ord('k'), timingVariance = 10)
        while True:
            print layer.receiveFrame(deadline = time.time() + 100.0)

    if mode == 's':
        myId = ord('A') + int(time.time() * 100.) % 10
        print >> sys.stderr, 'My id is', myId
        layer = SecondLayer(nic, myId = myId)
        layer.doSync()

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage:', sys.argv[0], '[r|s]'
        sys.exit(1)
        
    main(sys.argv[1])
