#!/usr/bin/python
# -*- encoding: utf-8

import itertools, iinic

def main():
    nic = iinic.NIC(iinic.NetComm())

    msg1 = 'a' * 31
    msg2 = 'a' * 613

    for i in itertools.count(1):
        msg =  msg1 if i % 23 != 0 else msg2
        print 'i = %d, msg len = %d' % (i, len(msg))
        nic.tx(msg, overrun_fail=False)

if __name__ == '__main__':
    main()
