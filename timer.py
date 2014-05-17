#!/usr/bin/python
# -*- encoding: utf-8

import iinic

def main():
    nic = iinic.NIC(iinic.USBComm())
    i = 0
    while True:
        pings = []
        for j in xrange(10):
            i += 1
            nic.timing(10000 * i)
            pings.append(nic.ping())
        for p in pings:
            p.await()

if __name__ == '__main__':
    main()
