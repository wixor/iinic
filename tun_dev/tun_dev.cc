#include <csignal>
#include <cstdio>
#include <cstring>

#include <iostream>

#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/stat.h>
#include <sys/un.h>

#include <linux/if_tun.h>

#include "tun_dev.hh"

tun_dev::tun_dev(boost::asio::io_service& io_service)
	:
	_mtu(0),
	_fd(-1),
	_io_service(io_service),
	_desc(io_service)
{
	fd_guard fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		auto errstr = strerror(errno);
		throw std::string { "could not open tun device: " } + errstr;
	}

	memset(&_ifr, 0, sizeof(_ifr));
	_ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	int err = -1;
	for (unsigned i = 0; i < 9 && err == -1; i++) {
		snprintf(_ifr.ifr_name, sizeof(_ifr.ifr_name), "tun%u", i);
		err = ioctl(fd, TUNSETIFF, &_ifr);
	}
	if (err == -1) {
		auto errstr = strerror(errno);
		throw std::string { "could not open tun device: " } + errstr;
	}

	_fd = fd.disarm();
	_desc.assign(_fd);
}

tun_dev::~tun_dev()
{
	close(_fd);
}

void
tun_dev::set_ipv4(const std::string& ipv4)
{
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = AF_INET;

	addrinfo* ai;
	auto err = getaddrinfo(ipv4.c_str(), nullptr, &hints, &ai);
	if (err || !ai) {
		throw std::string { "could not translate ipv4 address: " }
			+ gai_strerror(err);
	}

	memcpy(&_ifr.ifr_addr, ai->ai_addr, sizeof(_ifr.ifr_addr));
	freeaddrinfo(ai);

	auto s = socket();
	err = ioctl(s, SIOCSIFADDR, &_ifr);
	if (err == -1) {
		auto errstr = strerror(errno);
		throw std::string { "could not set tun ipv4 address: " } + errstr;
	}
}

void
tun_dev::set_mtu(unsigned mtu)
{
	auto s = socket();
	_ifr.ifr_mtu = mtu;
	auto err = ioctl(s, SIOCSIFMTU, &_ifr);
	if (err == 1) {
		auto errstr = strerror(errno);
		throw std::string { "could not set mtu: " } + errstr;
	}
	_mtu = mtu;
}

void
tun_dev::set_up(bool up)
{
	auto s = socket();
	auto err = ioctl(s, SIOCGIFFLAGS, &_ifr);
	if (err == -1) {
		auto errstr = strerror(errno);
		throw std::string { "could not get tun flags: " } + errstr;
	}
	if (up)
		_ifr.ifr_flags |= IFF_UP;
	else
		_ifr.ifr_flags &= ~IFF_UP;
	err = ioctl(s, SIOCSIFFLAGS, &_ifr);
	if (err == -1) {
		auto errstr = strerror(errno);
		throw std::string { "could not set up tun: " } + errstr;
	}
}

void
tun_dev::send(buffer& buf)
{
	_desc.write_some(boost::asio::buffer(buf, buf.size()));
}

void
tun_dev::listen()
{
	auto data = new char[_mtu];
	_desc.async_read_some(boost::asio::buffer(data, _mtu),
		[=](boost::system::error_code ec, size_t len)
		{
			buffer buf(data, len);
			if (ec)
				throw std::string { "error reading from tun: " } + ec.message();
			relay_buf(buf);
			listen();
		});
}

fd_guard
tun_dev::socket() const
{
	fd_guard s = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		auto errstr = strerror(errno);
		throw std::string { "could not create socket: " } + errstr;
	}
	return s;
}

