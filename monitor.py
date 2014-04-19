#!/usr/bin/python

import iinic

def main():
    nic = iinic.NIC(iinic.NetComm())

    while True:
        print nic.rx()

if __name__ == '__main__':
    main()

