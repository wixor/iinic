#ifndef UNIX_SCK_HH
#define UNIX_SCK_HH

#include <string>

#include <boost/asio.hpp>

#include "buffer.hh"

class unix_sck final : public buf_relay {
public:
	explicit unix_sck(boost::asio::io_service& io_service);

	void set_max_size(size_t max) {
		_max_size = max;
	}

	std::string name() const { return _name; }

	void send(buffer& buf) override;
	void listen();

private:
	void listen_connected();

	uint32_t _init;
	bool _connected;
	boost::asio::local::datagram_protocol::endpoint _client;

	size_t _max_size;

	std::string _name;

	boost::asio::io_service& _io_service;
	boost::asio::local::datagram_protocol::socket _sock;
};

#endif
