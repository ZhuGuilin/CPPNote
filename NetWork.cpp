#include <iostream>
#include <thread>
#include <chrono>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <windows.h>
#include "NetWork.h"

#pragma comment(lib, "ws2_32.lib")


#if NTDDI_VERSION >= NTDDI_WIN8
constexpr DWORD WSA_FLAGS = WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED;
#else
constexpr DWORD WSA_FLAGS = WSA_FLAG_OVERLAPPED;
#endif

sockaddr_in MakeAddress(const std::string& ip, const uint16_t port) 
{
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = ::htons(port);

	if (ip.empty() || ip == "*") {
		addr.sin_addr.s_addr = INADDR_ANY;
	}
	else {
		::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
	}

	return addr;
}

struct WinSockInitializer 
{
	WinSockInitializer() 
	{
		WSADATA wsaData;
		::WSASetLastError(0);
		if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw std::system_error(::WSAGetLastError(),
				std::system_category(),
				"WSAStartup failed");
		}
	}

	~WinSockInitializer() 
	{
		WSACleanup();
	}
};

class ThreadGuardJoin
{
public:

	explicit ThreadGuardJoin(std::thread&& t) noexcept(false)
		: _t(std::move(t))
	{
		if (!_t.joinable())
			throw std::logic_error("no thread!");
	}

	ThreadGuardJoin(ThreadGuardJoin&& other) noexcept
		: _t(std::move(other._t))
	{
	}

	virtual ~ThreadGuardJoin()
	{
		if (_t.joinable())
			_t.join();
	}

	ThreadGuardJoin(const ThreadGuardJoin&) = delete;
	ThreadGuardJoin& operator=(const ThreadGuardJoin&) = delete;
	ThreadGuardJoin& operator=(ThreadGuardJoin&& other) = delete;

private:

	std::thread _t;
};


NetWork::Socket::Socket(Service& service, int af, Type type)
	: _service(service)
	, _type(type)
{
	//	创建套接字
	_socket = ::WSASocketW(af,
						   type == Type::TCP ? SOCK_STREAM : SOCK_DGRAM,
						   type == Type::TCP ? IPPROTO_TCP : IPPROTO_UDP,
						   nullptr,
						   0,
						   WSA_FLAGS);
	if (INVALID_SOCKET == _socket)
	{
		std::cerr << "NetWork::Scoket::Scoket => WSASocketW() failed! error : " 
				  << ::WSAGetLastError() << std::endl;
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
								std::system_category()), "NetWork::Scoket::Scoket => WSASocketW() failed!");
	}

	//	关联IOCP
	if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(_socket), reinterpret_cast<ULONG_PTR>(this)))
	{ 
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
			std::system_category()), "NetWork::Scoket::Scoket => RegisterHandle() failed!");
	}

	//	获取扩展函数指针

	GUID guidConnectEx = WSAID_CONNECTEX;
	DWORD bytes = 0;
	if (SOCKET_ERROR == ::WSAIoctl(_socket,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guidConnectEx, sizeof(guidConnectEx),
								   &_connectEx, sizeof(_connectEx),
								   &bytes, nullptr, nullptr))
	{
		std::cerr << "NetWork::Scoket::Scoket => WSAIoctl() get ConnectEx failed! error : "
				  << ::WSAGetLastError() << std::endl;
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
								std::system_category()), "NetWork::Scoket::Scoket => WSAIoctl() get ConnectEx failed!");
	}

	GUID guidAcceptEx = WSAID_ACCEPTEX;
	bytes = 0;
	if (SOCKET_ERROR == ::WSAIoctl(_socket,
								SIO_GET_EXTENSION_FUNCTION_POINTER,
								&guidAcceptEx, sizeof(guidAcceptEx),
								&_acceptEx, sizeof(_acceptEx),
								&bytes, nullptr, nullptr))
	{
		std::cerr << "NetWork::Scoket::Scoket => WSAIoctl() get AcceptEx failed! error : "
			<< ::WSAGetLastError() << std::endl;
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
			std::system_category()), "NetWork::Scoket::Scoket => WSAIoctl() get AcceptEx failed!");
	}

}

//NetWork::Socket::Socket(Service& service, SOCKET s, int af, Type type)
//	:_socket(s), _type(type), _service(service)
//{
//	//	关联IOCP
//	if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(_socket), reinterpret_cast<ULONG_PTR>(this)))
//	{
//		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
//			std::system_category()), "NetWork::Scoket::Scoket => RegisterHandle() failed!");
//	}
//
//	//	获取扩展函数指针
//	GUID guidConnectEx = WSAID_CONNECTEX;
//	DWORD bytes = 0;
//	if (SOCKET_ERROR == ::WSAIoctl(_socket,
//								SIO_GET_EXTENSION_FUNCTION_POINTER,
//								&guidConnectEx,
//								sizeof(guidConnectEx),
//								&_connectEx,
//								sizeof(_connectEx),
//								&bytes,
//								nullptr,
//								nullptr))
//	{
//		std::cerr << "NetWork::Scoket::Scoket => WSAIoctl() get ConnectEx failed! error : "
//				  << ::WSAGetLastError() << std::endl;
//		::closesocket(_socket);
//		_socket = INVALID_SOCKET;
//		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
//			std::system_category()), "NetWork::Scoket::Scoket => WSAIoctl() get ConnectExfailed!");
//	}
//
//	GUID guidAcceptEx = WSAID_ACCEPTEX;
//	bytes = 0;
//	if (SOCKET_ERROR == ::WSAIoctl(_socket,
//								SIO_GET_EXTENSION_FUNCTION_POINTER,
//								&guidAcceptEx,
//								sizeof(guidAcceptEx),
//								&_acceptEx,
//								sizeof(_acceptEx),
//								&bytes,
//								nullptr,
//								nullptr))
//	{
//		std::cerr << "NetWork::Scoket::Scoket => WSAIoctl() get AcceptEx failed! error : "
//			<< ::WSAGetLastError() << std::endl;
//		::closesocket(_socket);
//		_socket = INVALID_SOCKET;
//		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
//			std::system_category()), "NetWork::Scoket::Scoket => WSAIoctl() get AcceptEx failed!");
//	}
//}

NetWork::Socket::~Socket()
{
	Reset();
}

void NetWork::Socket::Reset()
{
	if (INVALID_SOCKET != _socket)
	{
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
	}

	_accept_op.reset();
	_connect_op.reset();
	_read_op.reset();
	_write_op.reset();
}

void NetWork::Socket::Bind(const std::string& ip, const uint16_t port)
{
	sockaddr_in addr = MakeAddress(ip, port);
	if (::bind(_socket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) 
	{
		std::cerr << "NetWork::Socket::Bind => bind() failed! error : "
				  << ::WSAGetLastError() << std::endl;
	}
}

void NetWork::Socket::Listen(int backlog)
{
	if (_type != Type::TCP)
	{
		std::cerr << ("Listen only for TCP sockets") << std::endl;
	}

	if (::listen(_socket, backlog) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Socket::Listen => listen() failed! error : "
				  << ::WSAGetLastError() << std::endl;
	}
}

std::shared_ptr<NetWork::Socket> NetWork::Socket::AsyncAccept(void* buffer, size_t size, 
	std::function<void(std::error_code, size_t)>&& handler)
{
	if (_type != Type::TCP)
	{
		std::cerr << ("AsyncAccept only for TCP sockets") << std::endl;
		return nullptr;
	}
	
	if (_accept_op)
	{
		std::cerr << "NetWork::Socket::AsyncAccept => already accepting!" << std::endl;
		return nullptr;
	}

	_accept_op = std::make_unique<AcceptOperation>(handler);
	auto client = std::make_shared<Socket>(_service, AF_INET, Type::TCP);
	if (!(reinterpret_cast<LPFN_ACCEPTEX>(_acceptEx))(_socket,
									client->Handle(),
									buffer,
									0,
									sizeof(sockaddr_in) + 16,
									sizeof(sockaddr_in) + 16,
									(DWORD*) &size,
									_accept_op.get()))
	{
		std::cerr << "NetWork::Socket::AsyncAccept => AcceptEx() failed! error : "
				  << ::WSAGetLastError() << std::endl;
		return nullptr;
	}
	
	return client;
}

void NetWork::Socket::AsyncConnect(const std::string& host, const uint16_t port,
	std::function<void(std::error_code)>&& handler)
{
	if (_type != Type::TCP)
	{
		std::cerr << ("Connect only for TCP sockets") << std::endl;
	}

	if (_connect_op)
	{
		std::cerr << "NetWork::Socket::AsyncConnect => already connecting!" << std::endl;
		return;
	}

	if (!_connectEx)
	{
		std::cerr << "NetWork::Socket::AsyncConnect => ConnectEx() not supported!" << std::endl;
		return;
	}

	_connect_op = std::make_unique<ConnectOperation>(std::move(handler));
	sockaddr_in addr = MakeAddress(host, port);
	(reinterpret_cast<LPFN_CONNECTEX>(_connectEx))(_socket,
							  reinterpret_cast<SOCKADDR*>(&addr),
							  sizeof(addr),
							  nullptr,
							  0,
							  nullptr,
							  reinterpret_cast<LPWSAOVERLAPPED>(_connect_op.get()));
}

void NetWork::Socket::AsyncRead(void* buffer, size_t size,
	std::function<void(std::error_code, size_t)>&& handler)
{
	if (_type != Type::TCP)
	{
		std::cerr << ("Read only for TCP sockets") << std::endl;
	}

	if (_read_op)
	{
		std::cerr << "NetWork::Socket::AsyncRead => already reading!" << std::endl;
		return;
	}

	_read_op = std::make_unique<ReadOperation>(std::move(handler));
	WSABUF wsabuf = { static_cast<ULONG>(size), static_cast<CHAR*>(buffer) };
	DWORD flags = 0;
	int result = ::WSARecv(_socket,
						   &wsabuf,
						   1,
						   nullptr,
						   &flags,
						   _read_op.get(),
						   nullptr);
	if (result == SOCKET_ERROR)
	{
		int err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			std::cerr << "NetWork::Socket::AsyncRead => WSARecv() failed! error : "
					  << err << std::endl;
			_read_op->Complete(std::error_code(err, std::system_category()), 0);
			_read_op.reset();
			return;
		}
	}
	else
	{
		std::cout << "read comlete buffer :" << buffer << std::endl;
	}
}

void NetWork::Socket::AsyncWrite(const void* data, size_t size,
	std::function<void(std::error_code, size_t)>&& handler)
{
	if (_type != Type::TCP)
	{
		std::cerr << ("Write only for TCP sockets") << std::endl;
	}

	if (_write_op)
	{
		std::cerr << "NetWork::Socket::AsyncWrite => already writing!" << std::endl;
		return;
	}

	_write_op = std::make_unique<WriteOperation>(std::move(handler));
	WSABUF wsabuf = { static_cast<ULONG>(size), const_cast<char*>(static_cast<const char*>(data)) };
	DWORD flags = 0;
	int result = ::WSASend(_socket,
						   &wsabuf,
						   1,
						   nullptr,
						   flags,
						   _write_op.get(),
						   nullptr);
	if (result == SOCKET_ERROR)
	{
		int err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			std::cerr << "NetWork::Socket::AsyncWrite => WSASend() failed! error : "
					  << err << std::endl;
			_write_op->Complete(std::error_code(err, std::system_category()), 0);
			_write_op.reset();
			return;
		}
	}
}

NetWork::Service::Service()
	:_stopped(false)
{
	_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (INVALID_HANDLE_VALUE == _iocp)
	{
		std::cerr << "NetWork::Service::Service => CreateIoCompletionPort() failed! error : "
				  << ::GetLastError() << std::endl;
		throw std::system_error(std::error_code(static_cast<int>(::GetLastError()),
								std::system_category()), "NetWork::Service::Service => CreateIoCompletionPort() failed!");
	}
}

NetWork::Service::~Service()
{
	if (INVALID_HANDLE_VALUE != _iocp)
	{
		::CloseHandle(_iocp);
		_iocp = INVALID_HANDLE_VALUE;
	}
}

void NetWork::Service::Post(Operation* op) noexcept
{
	if (_stopped.load())
	{
		if (op)
		{
			op->Complete(std::make_error_code(std::errc::operation_canceled), 0);
		}
		return;
	}

	if (!::PostQueuedCompletionStatus(_iocp, 0, 0, op))
	{
		//	失败了, op需要存储
		std::cerr << "NetWork::Service::post => PostQueuedCompletionStatus() failed! error : "
					<< ::GetLastError() << std::endl;

	}
}

bool NetWork::Service::RegisterHandle(HANDLE handle, ULONG_PTR key) noexcept
{
	if (::CreateIoCompletionPort(handle, _iocp, key, 0) == 0)
	{
		std::cerr << "NetWork::Service::associate_handle => CreateIoCompletionPort() failed! error : "
				  << ::GetLastError() << std::endl;
		return false;
	}
	return true;
}

void NetWork::Service::Stop() noexcept
{ 
	_stopped.store(true);
	::PostQueuedCompletionStatus(_iocp, 0, 0, nullptr);
}

void NetWork::Service::run()
{
	DWORD bytes_transferred = 0;
	ULONG_PTR completion_key = 0;
	LPOVERLAPPED overlapped = 0;
	DWORD last_error = 0;

	while (!_stopped.load())
	{
		::SetLastError(0);
		BOOL ok = ::GetQueuedCompletionStatus(_iocp,
											&bytes_transferred,
											&completion_key,
											&overlapped,
											INFINITE);
		last_error = ::GetLastError();
		std::cout << "GetQueuedCompletionStatus => ok : " << ok
				  << ", bytes_transferred : " << bytes_transferred
				  << ", completion_key : " << completion_key
				  << ", overlapped : " << overlapped
				  << ", last_error : " << last_error
				  << std::endl;
		if (_stopped.load())
			break;

		if (overlapped)
		{
			Operation* op = static_cast<Operation*>(overlapped);
			op->Complete(std::error_code(), bytes_transferred);
		}
		else if (!ok)
		{

		}
	}
}

void NetWork::Test()
{
	std::cout << " ===== NetWork Bgein =====" << std::endl;

	std::cout << "WriteOperation sizeof :" << sizeof(WriteOperation) << std::endl;
	std::cout << "Scoket sizeof :" << sizeof(Socket) << std::endl;
	std::cout << "Service sizeof :" << sizeof(Service) << std::endl;

	WinSockInitializer wsaInit;
	(void)wsaInit;	//	避免编译器警告

	NetWork::Service service;
	ThreadGuardJoin task(std::thread([&service]() {
		service.run();
		}));
	(void)task;	//	避免编译器警告
	NetWork::Socket server(service, AF_INET, NetWork::Socket::Type::TCP);
	server.Bind("0.0.0.0", 18080);
	server.Listen();
	
#if 1
	// 异步接受连接
	NetWork::ReadOperation* read_op = nullptr;
	auto accept_handler = [&](std::shared_ptr<Socket> client) {
		if (!client) return;
		
		// 异步读取数据
		char buffer[1024];
		auto f = [&](std::error_code ec, size_t bytes) {
			std::cout << "Read completed with " << ec.message() << " bytes: " << bytes << std::endl;
			if (!ec && bytes > 0) {
				printf("Received %zu bytes: %.*s\n",
					bytes, static_cast<int>(bytes), buffer);

				// 回显数据
				client->AsyncWrite(buffer, bytes,
					[](std::error_code ec, size_t bytes) {
						if (!ec) {
							printf("Sent %zu bytes\n", bytes);
						}
				});
			}
			else {
				std::cout << "Client disconnected or read error: " << ec.message() << std::endl;
			}
		};

		client->AsyncRead(buffer, sizeof(buffer), std::move(f));
		//read_op = new NetWork::ReadOperation(std::move(f));
		//service.Post(read_op);
	};

	// 接受第一个连接
	char Acceptbuf[1024];
	std::size_t Acceptbuflen = sizeof(Acceptbuf);
	std::shared_ptr<Socket> client_backup = server.AsyncAccept(Acceptbuf, Acceptbuflen, [&](std::error_code ec, size_t bytes) {
		std::cout << "Accept completed with " << ec.message() << " bytes: " << bytes << std::endl;
		});
	accept_handler(client_backup);

#else
	auto client = server.Accept();

#endif
	using namespace std::chrono_literals;
	for (;;)
		std::this_thread::sleep_for(50ms);
	service.Stop();
	
	std::cout << " ===== NetWork End =====" << std::endl;
}
