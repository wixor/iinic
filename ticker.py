#!/usr/bin/python

import sys, itertools, iinic

def main():
    if len(sys.argv) != 2:
        print 'usage: ticker.py [a|b]'
        sys.exit(1)

    nic = iinic.NIC(iinic.NetComm())

    msg = sys.argv[1]*10 + '\n'
    delay = 1000000 if sys.argv[1] == 'a' else 850000

    for i in itertools.count(1):
        nic.timing(delay * i)
        nic.tx(msg)
        nic.sync()

if __name__ == '__main__':
    main()
