

#include "unix_sck.hh"

unix_sck::unix_sck(boost::asio::io_service& io_service)
	:
	_connected(false),
	_max_size(4096),
	_name(tmpnam(nullptr)),
	_io_service(io_service),
	_sock(io_service)
{
	boost::asio::local::datagram_protocol::endpoint ep(_name);

	_sock.open();
	_sock.bind(ep);
}

void
unix_sck::send(buffer& buf_ref)
{
	buffer buf = std::move(buf_ref);

	if (!_connected) {
		std::cout << "unix socket not connected, dropping " << buf.size()
			<< " bytes\n";
		return;
	}

	_sock.send(boost::asio::buffer(buf, buf.size()));
}

void
unix_sck::listen()
{
	_sock.async_receive_from(boost::asio::buffer(&_init, sizeof(_init)),
		_client, [this](boost::system::error_code ec, size_t)
		{
			if (ec) {
				throw std::string { "error reading from unix socket: " }
					+ ec.message();
			}

			_sock.connect(_client);
			_connected = true;

			listen_connected();
		});
}

void
unix_sck::listen_connected()
{
	auto data = new char[_max_size];
	_sock.async_receive(boost::asio::buffer(data, _max_size),
		[=](boost::system::error_code ec, size_t len)
		{
			buffer buf(data, len);
			if (ec) {
				throw std::string { "error reading from unix socket: " }
					+ ec.message();
			}

			relay_buf(buf);
			listen_connected();
		});
}
