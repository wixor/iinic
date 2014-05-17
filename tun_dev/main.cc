#include <iostream>

#include "buffer.hh"
#include "tun_dev.hh"
#include "unix_sck.hh"

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " IPv4_ADDRESS\n";
		return 1;
	}

	try {
		boost::asio::io_service ios;

		tun_dev dev(ios);
		std::cout << "tun device: " << dev.name() << "\n";

		unix_sck ux(ios);
		std::cout << "unix socket: " << ux.name() << "\n";

		dev.set_ipv4(argv[1]);
		dev.set_mtu(196);
		dev.set_up();
		dev.set_next(ux);
		dev.listen();

		ux.set_next(dev);
		ux.listen();

		ios.run();
	}
	catch (std::string msg) {
		std::cerr << msg << "\n";
	}
}
