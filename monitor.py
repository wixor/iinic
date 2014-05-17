#!/usr/bin/python

import iinic

def main():
    nic = iinic.NIC(iinic.NetComm())
    nic.set_bitrate(nic.BITRATE_600)

    while True:
        print nic.rx()

if __name__ == '__main__':
    main()

