#include <print>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <windows.h>
#include "NetWork.h"

#pragma comment(lib, "ws2_32.lib")


GUID WSAID_AcceptEx = WSAID_ACCEPTEX;
GUID WSAID_ConnectEx = WSAID_CONNECTEX;

#if 0//NTDDI_VERSION >= NTDDI_WIN8		//	这里会导致WSARecv报10083的错误
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

static struct WinSockInitializer
{
	WinSockInitializer()
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
} s_win_sock_initializer;

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
		std::print("NetWork::Acceptor::Acceptor => WSASocket() failed! error : {}.\n",
			::WSAGetLastError());
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
	
	_opt = std::make_unique<Operation>();
	_opt->op_type = Operation::Type::Type_Accept;

	std::print("NetWork::Acceptor::Acceptor => Acceptor created! \n listening on => {}:{}\n listen socket => {}.\n",
				_addr.to_string(), _port, _listener);
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
	auto client = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAGS);
	DWORD dwRecvNumBytes = 0;
	std::memset(_opt.get(), 0, sizeof(OVERLAPPED));
	_opt->socket = client;
	auto ret = reinterpret_cast<LPFN_ACCEPTEX>(_lpfnAcceptEx)(
				_listener,
				client,
				_readBuffer.data(),
				0,
				sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16,
				&dwRecvNumBytes,
				_opt.get());

	auto last_error = ::WSAGetLastError();
	if (!ret && last_error != WSA_IO_PENDING)
	{
		std::print("NetWork::Socket::AsyncAccept => AcceptEx() failed! error : {}, client socket : {}.\n",
			::WSAGetLastError(), client);
	}
	else
	{
		std::print("NetWork::Acceptor::AsyncAccept => AcceptEx() posted! socket => {}.\n", client);
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
		std::print("NetWork::Acceptor::Acceptor => setsockopt() SO_REUSEADDR failed! error : {}.\n", ::WSAGetLastError());
		return false;
	}

	//	启用条件接受
	int conditional = 1;
	if (::setsockopt(_listener, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, (char*)&conditional, sizeof(conditional)) == SOCKET_ERROR)
	{
		std::print("NetWork::Acceptor::Acceptor => setsockopt() SO_CONDITIONAL_ACCEPT failed! error : {}.\n", ::WSAGetLastError());
	}

	LINGER lingerOpt;
	lingerOpt.l_onoff = 1;   // 启用 linger
	lingerOpt.l_linger = 30; // 最多等待30秒
	if (::setsockopt(_listener, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt)) == SOCKET_ERROR)
	{
		std::print("NetWork::Acceptor::Acceptor => setsockopt() SO_LINGER failed! error : {}.\n", ::WSAGetLastError());
	}

	//	禁用 Nagle 算法
	int noDelay = 1;
	if (::setsockopt(_listener, IPPROTO_TCP, TCP_NODELAY, (char*)&noDelay, sizeof(noDelay)) == SOCKET_ERROR)
	{
		std::print("NetWork::Acceptor::Acceptor => setsockopt() TCP_NODELAY failed! error : {}.\n", ::WSAGetLastError());
		return false;
	}

	return true;
}

std::vector<std::shared_ptr<NetWork::TcpSocket>> _sockets;	//	用于测试，保存连接对象，防止被释放
void NetWork::Acceptor::OnAcceptComplete(const std::error_code& ec, const std::size_t size, SOCKET socket)
{
	std::print("Accept completed socket : {}, with : {}, bytes : {}.\n", socket, ec.message(), size);
	if (::setsockopt(
		socket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)&_listener,
		sizeof(_listener)) == SOCKET_ERROR)
	{
		std::print("NetWork::Acceptor::OnAcceptComplete => setsockopt() SO_UPDATE_ACCEPT_CONTEXT failed! error : {}.\n",
					::WSAGetLastError());
	}

	//	创建新连接对象
	auto tcpSocket = std::make_shared<TcpSocket>(socket);
	_sockets.push_back(tcpSocket);

	//	新连接关联 IOCP
	if (!_service.RegisterHandle(reinterpret_cast<HANDLE>(socket),
		reinterpret_cast<ULONG_PTR>(tcpSocket.get())))
	{
		std::print("NetWork::Acceptor::OnAcceptComplete => RegisterHandle() failed! error : {}.\n", ::WSAGetLastError());
	}

	//	新连接投递读操作
	tcpSocket->AsyncRead();

	//	继续接受下一个连接
	if (!_closed.load())
	{ 
		AsyncAccept();
	}
}

bool NetWork::Acceptor::Bind()
{
	sockaddr_in addr = MakeAddress(_addr.to_string(), _port);
	if (SOCKET_ERROR == ::bind(_listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)))
	{
		std::print("NetWork::Acceptor::Bind => bind() failed! error : {}.\n", ::WSAGetLastError());
		return false;
	}

	if (SOCKET_ERROR == ::listen(_listener, SOMAXCONN))
	{
		std::print("NetWork::Acceptor::Bind => listen() failed! error : {}.\n", ::WSAGetLastError());
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
	_socket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAGS);
	if (_socket == INVALID_SOCKET)
	{
		std::print("NetWork::Acceptor::Acceptor => WSASocket() failed! error : {}.\n", ::WSAGetLastError());
	}
	else
	{
		initialize_socket();
	}
}

NetWork::TcpSocket::TcpSocket(SOCKET socket)
	: _socket(socket)
	, _state(State_Closed)
	, _remoteAddress(address_v4::any())
{
	initialize_socket();
}

NetWork::TcpSocket::~TcpSocket()
{
	std::print("NetWork::TcpSocket::~TcpSocket() socket : {}.\n", _socket);
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
	DWORD dwFlags = 0;
	DWORD dwRecved = 0;
	
	_readBuffer.reset();
	_wsabuf.len = _readBuffer.size();
	_wsabuf.buf = (int8_t*)_readBuffer.data();
	std::memset(_readOpt.get(), 0, sizeof(OVERLAPPED));
	::WSASetLastError(0);
	int result = ::WSARecv(_socket,
						(WSABUF*)&_wsabuf,
						1,
						&dwRecved,
						&dwFlags,
						_readOpt.get(),
						nullptr);
	int err = ::WSAGetLastError();
	if (result == SOCKET_ERROR && err != WSA_IO_PENDING)
	{
		std::print("NetWork::TcpSocket::AsyncRead => WSARecv() failed! error : {}, socket : {}.\n", err, _socket);
		OnReadComplete(std::error_code(err, std::system_category()), 0);
	}
}

void NetWork::TcpSocket::AsyncSend()
{
	DWORD dwFlags = 0;
	DWORD dwSend = 0;

	_wsabuf.len = _sendBuffer.read_size();
	_wsabuf.buf = (int8_t*)_sendBuffer.data();
	std::memset(_sendOpt.get(), 0, sizeof(OVERLAPPED));
	::WSASetLastError(0);
	int result = ::WSASend(_socket,
		(WSABUF*)&_wsabuf,
		1,
		&dwSend,
		dwFlags,
		_sendOpt.get(),
		nullptr);
	int err = ::WSAGetLastError();
	if (result == SOCKET_ERROR && err != WSA_IO_PENDING)
	{
		std::print("NetWork::TcpSocket::AsyncSend => WSASend() failed! error : {}, socket : {}.\n", err, _socket);
		OnSendComplete(std::error_code(err, std::system_category()), 0);
	}
}

void NetWork::TcpSocket::OnConnectComplete(const std::error_code& ec, const std::size_t size)
{
	std::print("NetWork::TcpSocket::OnConnectComplete => Connect completed with : {}, bytes : {}.\n",
			ec.message(), size);
}

void NetWork::TcpSocket::OnReadComplete(const std::error_code& ec, const std::size_t size)
{
	if (size == 0)
	{
		std::print("NetWork::TcpSocket::OnReadComplete => Connection closed by peer! socket : {}.\n", _socket);
		//shutdown();
		return;
	}

	//	继续投递读操作
	if (_state == State_Open)
	{
		AsyncRead();
	}

	std::string msg((char*)_readBuffer.data(), size);
	std::print("NetWork::TcpSocket::OnReadComplete => Read message : {}.\n", msg);

	//	echo back
	_sendBuffer.write(msg.data(), size);
	AsyncSend();
}

void NetWork::TcpSocket::OnSendComplete(const std::error_code& ec, const std::size_t size)
{
	std::print("NetWork::TcpSocket::OnSendComplete => Send completed with : {}, bytes : {}, socket : {}.\n",
			ec.message(), size, _socket);

	_sendBuffer.reset();
}

void NetWork::TcpSocket::initialize_socket()
{
	int nZero = 0;
	int noDelay = 1;
	int reAddr = 1;

	//	允许地址重用
	if (::setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reAddr, sizeof(reAddr)) == SOCKET_ERROR)
	{
		std::print("NetWork::Acceptor::Acceptor => setsockopt() SO_REUSEADDR failed! error : {}.\n", ::WSAGetLastError());
	}

	//	设置发送缓冲区为0，禁用缓冲区(减少延迟, 友好短消息)
	if (::setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&nZero, sizeof(nZero)) == SOCKET_ERROR)
	{
		std::print("NetWork::TcpSocket::TcpSocket => setsockopt() SO_SNDBUF failed! error : {}.\n", ::WSAGetLastError());
	}

	//	禁用 Nagle 算法	
	if (::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&noDelay, sizeof(noDelay)) == SOCKET_ERROR)
	{
		std::print("NetWork::TcpSocket::TcpSocket => setsockopt() TCP_NODELAY failed! error : {}.\n", ::WSAGetLastError());
	}

	_readOpt = std::make_unique<Operation>();
	_readOpt->op_type = Operation::Type::Type_Read;

	_sendOpt = std::make_unique<Operation>();
	_sendOpt->op_type = Operation::Type::Type_Send;

	_lpfnConnectEx = reinterpret_cast<LPFN_ACCEPTEX>(GetExtensionProcAddress(_socket, WSAID_ConnectEx));
	_state.store(State_Open);
}

NetWork::Service::Service()
	:_stopped(false)
{
	_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (INVALID_HANDLE_VALUE == _iocp)
	{
		std::print("NetWork::Service::Service => CreateIoCompletionPort() failed! error : {}.\n", ::GetLastError());
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

void NetWork::Service::Post(Operation* opt) noexcept
{
	if (_stopped.load())
	{
		return;
	}

	if (!::PostQueuedCompletionStatus(_iocp, 0, 0, opt))
	{
		//	失败了, op需要存储
		std::print("NetWork::Service::post => PostQueuedCompletionStatus() failed! error : {}.\n", ::GetLastError());

	}
}

bool NetWork::Service::RegisterHandle(HANDLE handle, ULONG_PTR key) noexcept
{
	if (::CreateIoCompletionPort(handle, _iocp, key, 0) == 0)
	{
		std::print("NetWork::Service::associate_handle => CreateIoCompletionPort() failed! error : {}.\n", ::GetLastError());
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
	Operation* opt = nullptr;
	DWORD last_error = 0;

	while (!_stopped.load())
	{
		::SetLastError(0);
		ok = ::GetQueuedCompletionStatus(_iocp,
										&bytes_transferred,
										&completionKey,
										(LPOVERLAPPED*)&opt,
										INFINITE);
		last_error = ::GetLastError();
		if (_stopped.load())
			break;

		//	退出消息
		if (completionKey == 0)
			break;

		if (opt)
		{
			switch (opt->op_type)
			{
			case Operation::Type::Type_Accept:
				{
				auto acceptor = reinterpret_cast<NetWork::Acceptor*>(completionKey);
				acceptor->OnAcceptComplete(ok ? std::error_code()
					: std::error_code(static_cast<int>(last_error), std::system_category()),
					bytes_transferred, opt->socket);
				}
				break;
			case Operation::Type::Type_Connect:
				{
					auto socket = reinterpret_cast<NetWork::TcpSocket*>(completionKey);
					socket->OnConnectComplete(ok ? std::error_code()
						: std::error_code(static_cast<int>(last_error), std::system_category()),
						bytes_transferred);
				}
				break;
			case Operation::Type::Type_Read:
				{
					auto socket = reinterpret_cast<NetWork::TcpSocket*>(completionKey);
					socket->OnReadComplete(ok ? std::error_code()
						: std::error_code(static_cast<int>(last_error), std::system_category()),
						bytes_transferred);
				}
				break;
			case Operation::Type::Type_Send:
				{
					auto socket = reinterpret_cast<NetWork::TcpSocket*>(completionKey);
					socket->OnSendComplete(ok ? std::error_code()
						: std::error_code(static_cast<int>(last_error), std::system_category()),
						bytes_transferred);
				}
				break;
			default:
				break;
			}
			
		}
		else if (!ok)
		{
			std::print("NetWork::Service::run => GetQueuedCompletionStatus() failed! error : {}.\n", last_error);
		}
	}
}

void NetWork::Test()
{

	std::print(" ===== NetWork Bgein =====\n");

	std::print("std::error_code sizeof : {}.\n", sizeof(std::error_code));
	std::print("Acceptor sizeof : {}.\n", sizeof(Acceptor));
	std::print("TcpSocket sizeof : {}.\n", sizeof(TcpSocket));
	std::print("Operation sizeof : {}.\n", sizeof(Operation));
	std::print("Service sizeof : {}.\n", sizeof(Service));

	std::unordered_map<SOCKET, std::shared_ptr<NetWork::TcpSocket>> socket_map;
	socket_map.reserve(4);
	SOCKET s_del = INVALID_SOCKET;
	{
		auto sptr1 = std::make_shared<NetWork::TcpSocket>();
		socket_map.insert({ sptr1->handle(), sptr1 });

		auto sptr2 = std::make_shared<NetWork::TcpSocket>();
		socket_map.insert({ sptr2->handle(), sptr2 });

		auto sptr3 = std::make_shared<NetWork::TcpSocket>();
		socket_map.insert({ sptr3->handle(), sptr3 });

		s_del = sptr2->handle();
	}
	
	auto it = socket_map.find(s_del);
	if (it != socket_map.end())
	{
		socket_map.erase(it);
	}

	NetWork::Service service;
	Acceptor acceptor(service, NetWork::address_v4::any(), 8086);

	std::vector<ThreadGuardJoin> threads;
	for (int i = 0; i < 4; ++i)
	{ 
		threads.emplace_back(std::thread([&service, &acceptor]() {
			acceptor.AsyncAccept();
			service.run();
			}));
	}
	std::this_thread::sleep_for(std::chrono::seconds(120));

#if 0
	
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
		std::print("Accept completed with " << ec.message() << " bytes: " << bytes << std::endl;
		
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
				std::print("Client disconnected or read error : {}, socket : {}.\n", ec.message(), client->Handle());
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
	std::print(" ===== NetWork End =====\n");
}
