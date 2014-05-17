#!/usr/bin/env python

import array
import os
import socket
import struct
import sys
import tempfile

MTU = 196

class tun_dev:
	def __init__(self, sck):
		self.sck = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
		tmp = '/tmp/tun-dev-sock' + str(os.getpid())
		try:
			os.remove(tmp)
		except:
			pass
		self.sck.bind(tmp)
		self.sck.connect(sck)

		p = struct.pack('l', 0)
		self.sck.send(p)

	def recv(self):
		return self.sck.recv(MTU)

def get_protocol(prot):
	return {
		1: 'icmp',
		2: 'igmp',
		6: 'tcp',
		17: 'udp',
	}.get(prot, '<unknown>')

def get_ipv4_addr(addr):
	i = struct.pack('L', addr)
	b = array.array('B')
	b.fromstring(i)
	s = "" + str(b[3]) + "." + str(b[2]) + "." + str(b[1]) + "." + str(b[0])
	return s

dev = tun_dev(sys.argv[1])
while True:
	p = dev.recv()
	t = struct.unpack_from("!HHHHBBHLL", p)
	src = get_ipv4_addr(t[7])
	dst = get_ipv4_addr(t[8])
	print "ipv4: got", t[1], "bytes, protocol:", get_protocol(t[5]), "from:", src, "to:", dst
