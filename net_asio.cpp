#include "net_asio.h"
#include <print>

#define HAS_BOOST_LIB 0

#if HAS_BOOST_LIB
#include <boost/asio.hpp>

namespace
{
	using boost::asio::ip::tcp;
}
#endif

net_asio::net_asio()
{
}

net_asio::~net_asio()
{
}

void net_asio::Test()
{
	std::print("===== net_asio Bgein =====\n");
#if 0

	std::print("boost::asio::io_context sizeof : {}.\n", sizeof(boost::asio::io_context));
	std::print("boost::asio::ip::tcp::socket sizeof : {}.\n", sizeof(tcp::socket));

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
							std::print("Server received : {}, size : {}.\n", std::string(buffer, bytes_transferred),
										bytes_transferred);
						}
						else
						{
							std::print("Server read error : {}.\n", ec.message());
						}
					});

				std::print("Server: Accepted connection.\n");
			}
			else
			{
				std::print("Server: Accept error : {}.\n", ec.message());
			}
		});

	/*tcp::socket client_socket(io_ctx);
	client_socket.connect(acceptor.local_endpoint());
	client_socket.send(boost::asio::buffer("Hello, net_asio!"));*/

	io_ctx.run();

#endif

	std::print("===== net_asio End =====\n");
}
