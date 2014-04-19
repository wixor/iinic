#!/usr/bin/python

import os, tty, termios, select, time
import iinic

class ExampleClient(object):
    def __init__(self, nic):
        self._nic = nic

        self._linebuf = ''
    
        self._poll = select.poll()
        self._poll.register(0, select.POLLIN)
        self._poll.register(nic._comm.fileno(), select.POLLIN)

    def loop(self):
        oldtermios = termios.tcgetattr(0)
        tty.setcbreak(0)
        try:
            while True:
                for fd, evt in self._poll.poll():
                    if 0 == fd:
                        if not self._drainStdin():
                            return 
                    elif self._nic._comm.fileno() == fd:
                        self._drainNIC()
        finally:
            termios.tcsetattr(0, termios.TCSAFLUSH, oldtermios)

    def _drainStdin(self):
        rd = os.read(0, 1024)
        if not rd:
            return False
        self._linebuf += rd

        while True:
            eol = self._linebuf.find('\n')
            if eol == -1:
                break
            self._nic.tx(self._linebuf[:eol])
            self._linebuf = self._linebuf[eol+1:]

        return True

    def _drainNIC(self):
        while True:
            b = self._nic.rx(deadline=0)
            if b is None:
                break
            os.write(1, b.byte)

if __name__ == '__main__':
    ExampleClient(iinic.NIC(iinic.NetComm())).loop()
