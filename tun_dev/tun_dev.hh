#ifndef TUN_DEV_HH
#define TUN_DEV_HH

#include <string>

#include <boost/asio.hpp>

#include <net/if.h>

#include "buffer.hh"

class tun_dev final : public buf_relay {
public:
	explicit tun_dev(boost::asio::io_service& io_service);
	~tun_dev();

	void set_ipv4(const std::string& ipv4);
	void set_mtu(unsigned mtu);
	void set_up(bool up = true);

	std::string name() const { return _ifr.ifr_name; }

	void send(buffer& buf) override;
	void listen();

private:
	fd_guard socket() const;

	unsigned _mtu;
	int _fd;
	ifreq _ifr;

	boost::asio::io_service& _io_service;
	boost::asio::posix::stream_descriptor _desc;
};

#endif
