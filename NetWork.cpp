#include <iostream>
#include <thread>
#include <chrono>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <windows.h>
#include "NetWork.h"

#pragma comment(lib, "ws2_32.lib")


GUID WSAID_AcceptEx = WSAID_ACCEPTEX;
GUID WSAID_ConnectEx = WSAID_CONNECTEX;

#if NTDDI_VERSION >= NTDDI_WIN8
constexpr DWORD WSA_FLAGS = WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED;
#else
constexpr DWORD WSA_FLAGS = WSA_FLAG_OVERLAPPED;
#endif

typedef std::vector<std::shared_ptr<NetWork::TcpSocket>> TcpSocketList;
TcpSocketList _sockets;

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

FARPROC GetExtensionProcAddress(SOCKET socket, GUID& Guid)
{
	FARPROC lpfn = NULL;
	DWORD dwBytes = 0;

	::WSAIoctl(socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&Guid,
		sizeof(Guid),
		&lpfn,
		sizeof(lpfn),
		&dwBytes,
		NULL,
		NULL);

	return lpfn;
}

struct WinSockInitializer
{
	explicit WinSockInitializer()
	{
		WSADATA wsaData;
		::WSASetLastError(0);
		if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			throw std::system_error(::WSAGetLastError(),
				std::system_category(), "WSAStartup failed");
		}
	}

	~WinSockInitializer()
	{
		::WSACleanup();
	}

	WinSockInitializer(const WinSockInitializer&) = delete;
	WinSockInitializer& operator=(const WinSockInitializer&) = delete;
	WinSockInitializer(WinSockInitializer&&) = default;
	WinSockInitializer& operator=(WinSockInitializer&&) = default;
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

NetWork::address_v4::address_v4(std::uint8_t addr[4]) noexcept
{
	_addr.S_un.S_un_b.s_b1 = addr[0];
	_addr.S_un.S_un_b.s_b2 = addr[1];
	_addr.S_un.S_un_b.s_b3 = addr[2];
	_addr.S_un.S_un_b.s_b4 = addr[3];
}

NetWork::address_v4::address_v4(std::uint32_t addr) noexcept
{
	_addr.S_un.S_addr = ::htonl(addr);
}

NetWork::address_v4::address_v4(std::string_view addr) noexcept
{
	::inet_pton(AF_INET, addr.data(), &_addr);
}

inline std::string NetWork::address_v4::to_string() const noexcept
{
	char str[INET_ADDRSTRLEN] = { 0 };
	::inet_ntop(AF_INET, &_addr, str, INET_ADDRSTRLEN);
	return std::string(str);
}

NetWork::Acceptor::Acceptor(Service& service, const address_v4& addr, const std::uint32_t port)
	: _service(service)
	, _addr(addr)
	, _port(port)
	, _closed(true)
{
	_listener = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAGS);
	if (_listener == INVALID_SOCKET)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => WSASocket() failed! error : "
			<< ::WSAGetLastError() << std::endl;
		goto __Construct_Failed;
	}

	if (!ConfigureListeningSocket())
	{
		goto __Construct_Failed;
	}

	if (!Bind())
	{
		goto __Construct_Failed;
	}

	if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(_listener), reinterpret_cast<ULONG_PTR>(this)))
	{
		goto __Construct_Failed;
	}
	
	_opt = new Operation([this](std::error_code ec, std::size_t size) {
		this->OnAcceptComplete(ec, size);
		});

	std::cout << "NetWork::Acceptor::Acceptor => Acceptor created! \n listening on => "
				<< _addr.to_string() << ":" << _port << "\n listen socket => " << _listener <<std::endl;
	return;

__Construct_Failed:
	if (_listener != INVALID_SOCKET)
	{
		::closesocket(_listener);
		_listener = INVALID_SOCKET;
	}
}

NetWork::Acceptor::~Acceptor()
{
	shutdown();
}

void NetWork::Acceptor::AsyncAccept()
{
	auto client = std::make_shared<NetWork::TcpSocket>();
	_sockets.push_back(client);

	DWORD dwRecvNumBytes = 0;
	auto ret = reinterpret_cast<LPFN_ACCEPTEX>(_lpfnAcceptEx)(
				_listener,
				client->handle(),
				_readBuffer.data(),
				0,
				sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16,
				&dwRecvNumBytes,
				&_opt->overlapped);

	auto last_error = ::WSAGetLastError();
	if (!ret && last_error != WSA_IO_PENDING)
	{
		std::cerr << "NetWork::Socket::AsyncAccept => AcceptEx() failed! error : "
			<< ::WSAGetLastError() << " client->handle :" << client->handle() << std::endl;
	}
	else
	{
		std::cout << "NetWork::Acceptor::AsyncAccept => AcceptEx() posted! socket => " << client->handle() << std::endl;
	}
}

void NetWork::Acceptor::shutdown() noexcept
{
	if (_closed.exchange(true))
		return;

	if (_listener != INVALID_SOCKET)
	{
		::closesocket(_listener);
		_listener = INVALID_SOCKET;
	}
}

bool NetWork::Acceptor::ConfigureListeningSocket()
{
	//	允许地址重用 
	int reuseAddr = 1;
	if (::setsockopt(_listener, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => setsockopt() SO_REUSEADDR failed! error : "
			<< ::WSAGetLastError() << std::endl;
		return false;
	}

	//	启用条件接受
	int conditional = 1;
	if (::setsockopt(_listener, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, (char*)&conditional, sizeof(conditional)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => setsockopt() SO_CONDITIONAL_ACCEPT failed! error : "
			<< ::WSAGetLastError() << std::endl;
	}

	LINGER lingerOpt;
	lingerOpt.l_onoff = 1;   // 启用 linger
	lingerOpt.l_linger = 30; // 最多等待30秒
	if (::setsockopt(_listener, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => setsockopt() SO_LINGER failed! error : "
			<< ::WSAGetLastError() << std::endl;
	}

	//	禁用 Nagle 算法
	int noDelay = 1;
	if (::setsockopt(_listener, IPPROTO_TCP, TCP_NODELAY, (char*)&noDelay, sizeof(noDelay)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => setsockopt() TCP_NODELAY failed! error : "
			<< ::WSAGetLastError() << std::endl;
		return false;
	}

	return true;
}

void NetWork::Acceptor::OnAcceptComplete(std::error_code ec, std::size_t size)
{
	std::cout << "Accept completed with " << ec.message() << " bytes: " << size << std::endl;
	if (::setsockopt(
		_sockets[0]->handle(),
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)&_listener,
		sizeof(_listener)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::OnAcceptComplete => setsockopt() SO_UPDATE_ACCEPT_CONTEXT failed! error : "
					<< ::WSAGetLastError() << std::endl;
	}

	//	允许地址重用
	int reuseAddr = 1;
	if (::setsockopt(_sockets[0]->handle(),
		SOL_SOCKET,
		SO_REUSEADDR,
		(char*)&reuseAddr,
		sizeof(reuseAddr)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => setsockopt() SO_REUSEADDR failed! error : "
			<< ::WSAGetLastError() << std::endl;
	}

	//	新连接关联 IOCP
	if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(_sockets[0]->handle()),
		reinterpret_cast<ULONG_PTR>(_sockets[0].get())))
	{
		std::cerr << "NetWork::Acceptor::OnAcceptComplete => RegisterHandle() failed! error : "
					<< ::WSAGetLastError() << std::endl;
	}

	//	新连接投递读操作
	_sockets[0]->AsyncRead();

	//	继续接受下一个连接
	if (!_closed.load())
		this->AsyncAccept();
}

bool NetWork::Acceptor::Bind()
{
	sockaddr_in addr = MakeAddress(_addr.to_string(), _port);
	if (SOCKET_ERROR == ::bind(_listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)))
	{
		std::cerr << "NetWork::Acceptor::Bind => bind() failed! error : "
					<< ::WSAGetLastError() << std::endl;
		return false;
	}

	if (SOCKET_ERROR == ::listen(_listener, SOMAXCONN))
	{
		std::cerr << "NetWork::Acceptor::Bind => listen() failed! error : "
					<< ::WSAGetLastError() << std::endl;
		return false;
	}

	_lpfnAcceptEx = reinterpret_cast<LPFN_ACCEPTEX>(GetExtensionProcAddress(_listener, WSAID_AcceptEx));
	_closed.store(false);
	return true;
}

NetWork::TcpSocket::TcpSocket()
	: _state(State_Closed)
	, _remoteAddress(address_v4::any())
{
	int nZero = 0;
	_socket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAGS);
	if (_socket == INVALID_SOCKET)
	{
		std::cerr << "NetWork::Acceptor::Acceptor => WSASocket() failed! error : "
			<< ::WSAGetLastError() << std::endl;
		goto __Construct_Failed;
	}

	//	设置发送缓冲区为0，禁用缓冲区(减少延迟, 友好短消息)
	if (::setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nZero, sizeof(nZero)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::TcpSocket::TcpSocket => setsockopt() SO_SNDBUF failed! error : "
					<< ::WSAGetLastError() << std::endl;
		goto __Construct_Failed;
	}

	_readOpt = new Operation([this](std::error_code ec, std::size_t size) {
		this->OnReadComplete(ec, size);
		});

	_sendOpt = new Operation([this](std::error_code ec, std::size_t size) {
		this->OnSendComplete(ec, size);
		});

	_wsabuf.len = _readBuffer.size();
	_wsabuf.buf = (int8_t*)_readBuffer.data();

	_lpfnConnectEx = reinterpret_cast<LPFN_ACCEPTEX>(GetExtensionProcAddress(_socket, WSAID_ConnectEx));
	_state.store(State_Open);

	return;

__Construct_Failed:
	if (_socket != INVALID_SOCKET)
	{
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
}

NetWork::TcpSocket::~TcpSocket()
{
	std::cout << "NetWork::TcpSocket::~TcpSocket() socket :" << _socket << std::endl;
	shutdown();
}

void NetWork::TcpSocket::shutdown() noexcept
{
	if (_state.exchange(State_Closed))
		return;

	if (_socket != INVALID_SOCKET)
	{
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
}

void NetWork::TcpSocket::AsyncConnect(address_v4&& addr, const std::uint16_t port,
	std::function<void(std::error_code)>&& complete)
{

}

void NetWork::TcpSocket::AsyncRead()
{
	int error = 0;
	int len = sizeof(error);
	if (::getsockopt(_socket, SOL_SOCKET, SO_TYPE, (char*)&error, &len) == SOCKET_ERROR)
	{
		std::cout << "NetWork::TcpSocket::AsyncRead => getsockopt() failed! error : "
					<< ::WSAGetLastError() << std::endl;
	}

	DWORD dwFlags = 0;
	DWORD dwRecved = 0;
	int result = ::WSARecv(_socket,
						(WSABUF*)&_wsabuf,
						1,
						&dwRecved,
						&dwFlags,
						&_readOpt->overlapped,
						nullptr);
	int err = ::WSAGetLastError();
	if (result == SOCKET_ERROR && err != WSA_IO_PENDING)
	{
		std::cerr << "NetWork::TcpSocket::AsyncRead => WSARecv() failed! error : "
					<< err << "	socket :" << _socket << std::endl;
		OnReadComplete(std::error_code(err, std::system_category()), 0);
	}
}

void NetWork::TcpSocket::AsyncSend()
{

}

void NetWork::TcpSocket::OnConnectComplete(std::error_code ec, std::size_t size)
{
	std::cout << "NetWork::TcpSocket::OnConnectComplete => Connect completed with "
		<< ec.message() << " bytes: " << size << std::endl;
}

void NetWork::TcpSocket::OnReadComplete(std::error_code ec, std::size_t size)
{
	std::cout << "NetWork::TcpSocket::OnReadComplete => Read completed with "
		<< ec.message() << " bytes: " << size << " socket :" << _socket << std::endl;
}

void NetWork::TcpSocket::OnSendComplete(std::error_code ec, std::size_t size)
{
	std::cout << "NetWork::TcpSocket::OnSendComplete => Send completed with "
		<< ec.message() << " bytes: " << size << " socket :" << _socket << std::endl;
}






#if 0

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
	/*if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(_socket), reinterpret_cast<ULONG_PTR>(this)))
	{ 
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
			std::system_category()), "NetWork::Scoket::Scoket => RegisterHandle() failed!");
	}*/
}

inline void NetWork::Socket::SetConnectPtr()
{
	if (_connectEx == nullptr)
	{
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
	}
}

inline void NetWork::Socket::SetAcceptPtr()
{
	if (_acceptEx == nullptr)
	{
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		DWORD bytes = 0;
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

void NetWork::Socket::UpdateSocket(SOCKET client_sock)
{
	if (::setsockopt(client_sock,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		reinterpret_cast<const char*>(&_socket),
		sizeof(_socket)) == SOCKET_ERROR)
	{
		std::cerr << "NetWork::Socket::UpdateSocket => setsockopt() failed! error : "
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
	auto client = std::make_shared<NetWork::Socket>(_service, AF_INET, Type::TCP);

	BOOL ret = (reinterpret_cast<LPFN_ACCEPTEX>(_acceptEx))(_socket,
				client->Handle(),
				buffer,
				0,
				sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16,
				(DWORD*)&size,
				_accept_op.get());
	auto last_error = ::WSAGetLastError();

	if (!ret && last_error != WSA_IO_PENDING)
	{
		std::cerr << "NetWork::Socket::AsyncAccept => AcceptEx() failed! error : "
				  << ::WSAGetLastError() << std::endl;
		return nullptr;
	}
	std::cout << "NetWork::Socket::AsyncAccept => AcceptEx() posted! socket :" << client->Handle() << std::endl;
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

	int error = 0;
	int len = sizeof(error);
	if (::getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == SOCKET_ERROR) 
	{
		std::cout << "NetWork::Socket::AsyncRead => getsockopt() failed! error : "
			<< ::WSAGetLastError() << std::endl;
	}

	_read_op = std::make_unique<ReadOperation>(std::move(handler));
	_wsabuf = { static_cast<ULONG>(size), static_cast<CHAR*>(buffer) };
	DWORD dwFlags = 0;
	DWORD dwRecved = 0;
	int result = ::WSARecv(_socket,
						   (WSABUF*)&_wsabuf,
						   1,
						   &dwRecved,
						   &dwFlags,
						   _read_op.get(),
						   nullptr);
	if (result == SOCKET_ERROR)
	{
		int err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			std::cerr << "NetWork::Socket::AsyncRead => WSARecv() failed! error : "
					  << err << "	socket :" << _socket << std::endl;
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
	_wsabuf = { static_cast<ULONG>(size), const_cast<char*>(static_cast<const char*>(data)) };
	DWORD flags = 0;
	int result = ::WSASend(_socket,
						   (WSABUF*)&_wsabuf,
						   (DWORD)size,
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

#endif

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
		return;
	}

	if (!::PostQueuedCompletionStatus(_iocp, 0, 0, &op->overlapped))
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
	BOOL ok;
	DWORD bytes_transferred = 0;
	ULONG_PTR completionKey = 0;
	//LPOVERLAPPED overlapped = 0;
	Operation* op = nullptr;
	DWORD last_error = 0;

	while (!_stopped.load())
	{
		::SetLastError(0);
		ok = ::GetQueuedCompletionStatus(_iocp,
										&bytes_transferred,
										&completionKey,
										//&overlapped,
										(LPOVERLAPPED*)&op,
										INFINITE);
		last_error = ::GetLastError();
		/*std::cout << "GetQueuedCompletionStatus => ok : " << ok
				  << ", bytes_transferred : " << bytes_transferred
				  << ", completion_key : " << completionKey
				  << ", overlapped hEvent : " << overlapped->hEvent
				  << ", last_error : " << last_error
				  << std::endl;*/
		if (_stopped.load())
			break;

		//	退出消息
		if (completionKey == 0)
			break;

		if (op)
		{
			//auto op = (Operation*)overlapped;
			op->complete(ok ? std::error_code()
								: std::error_code(static_cast<int>(last_error), std::system_category()),
				bytes_transferred);
		}
		else if (!ok)
		{

		}
	}
}

void NetWork::Test()
{

	std::cout << " ===== NetWork Bgein =====" << std::endl;
	WinSockInitializer wsaInit;
	(void)wsaInit;	//	避免编译器警告

	std::cout << "std::error_code sizeof :" << sizeof(std::error_code) << std::endl;
	std::cout << "Acceptor sizeof :" << sizeof(Acceptor) << std::endl;
	std::cout << "TcpSocket sizeof :" << sizeof(TcpSocket) << std::endl;
	std::cout << "Operation sizeof :" << sizeof(Operation) << std::endl;
	std::cout << "Service sizeof :" << sizeof(Service) << std::endl;

	NetWork::Service service;
	Acceptor acceptor(service, NetWork::address_v4::any(), 8086);
	acceptor.AsyncAccept();
	service.run();

#if 0
	std::cout << "WriteOperation sizeof :" << sizeof(WriteOperation) << std::endl;
	std::cout << "Scoket sizeof :" << sizeof(Socket) << std::endl;
	std::cout << "Service sizeof :" << sizeof(Service) << std::endl;
	std::cout << "WSABUF sizeof :" << sizeof(WSABUF) << std::endl;
	
	NetWork::Socket listener(service, AF_INET, NetWork::Socket::Type::TCP);
	listener.Bind("0.0.0.0", 8086);
	listener.Listen();
	listener.SetAcceptPtr();
	service.RegisterHandle(reinterpret_cast<HANDLE>(listener.Handle()), reinterpret_cast<ULONG_PTR>(&listener));
	
#if 1
	// 异步接受连接
	/*
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
	};
	*/

	// 接受第一个连接
	char Acceptbuf[1024];
	std::size_t Acceptbuflen = sizeof(Acceptbuf);
	std::shared_ptr<Socket> client = listener.AsyncAccept(Acceptbuf, Acceptbuflen, [&](std::error_code ec, size_t bytes) {
		std::cout << "Accept completed with " << ec.message() << " bytes: " << bytes << std::endl;
		
		listener.UpdateSocket(client->Handle());

		// 异步读取数据
		char buffer[1024];
		service.RegisterHandle(reinterpret_cast<HANDLE>(client->Handle()), reinterpret_cast<ULONG_PTR>(&client));
		auto f = [&](std::error_code ec, size_t bytes) {
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
				std::cout << "Client disconnected or read error: " << ec.message() 
					<< "	socket :" << client->Handle() << std::endl;
			}
		};

		client->AsyncRead(buffer, sizeof(buffer), std::move(f));
	});

#else
	char Acceptbuf[1024];
	std::size_t Acceptbuflen = sizeof(Acceptbuf);
	

#endif

	/*ThreadGuardJoin task(std::thread([&service]() {
		service.run();
		}));
	(void)task;*/

	service.run();

	/*using namespace std::chrono_literals;
	for (;;)
		std::this_thread::sleep_for(50ms);
	service.Stop();*/
#endif
	std::cout << " ===== NetWork End =====" << std::endl;
}
