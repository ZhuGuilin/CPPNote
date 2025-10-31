#include "net_asio.h"
#include <iostream>
#include <boost/asio.hpp>

namespace
{
	using boost::asio::ip::tcp;
}

net_asio::net_asio()
{
}

net_asio::~net_asio()
{
}

void net_asio::Test()
{
	std::cout << "===== net_asio Bgein =====" << std::endl;
#if 0

	std::cout << "boost::asio::io_context sizeof :" << sizeof(boost::asio::io_context) << std::endl;
	std::cout << "boost::asio::ip::tcp::socket sizeof :" << sizeof(tcp::socket) << std::endl;

	boost::asio::io_context io_ctx;
	tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8086);
	tcp::acceptor acceptor(io_ctx, endpoint);
	tcp::socket server_socket(io_ctx);
	acceptor.async_accept(server_socket,
		[&](const boost::system::error_code& ec)
		{
			if (!ec)
			{
				char buffer[1024];
				server_socket.async_read_some(boost::asio::buffer(buffer, 1024),
					[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
					{
						if (!ec)
						{
							std::cout << "Server received : " << std::string(buffer, bytes_transferred)
										<< "  size :" << bytes_transferred << std::endl;
						}
						else
						{
							std::cout << "Server read error: " << ec.message() << std::endl;
						}
					});

				std::cout << "Server: Accepted connection." << std::endl;
			}
			else
			{
				std::cout << "Server: Accept error: " << ec.message() << std::endl;
			}
		});

	/*tcp::socket client_socket(io_ctx);
	client_socket.connect(acceptor.local_endpoint());
	client_socket.send(boost::asio::buffer("Hello, net_asio!"));*/

	io_ctx.run();

#endif

	std::cout << "===== net_asio End =====" << std::endl;
}
