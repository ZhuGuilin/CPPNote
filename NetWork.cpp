#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include "NetWork.h"

#pragma comment(lib, "ws2_32.lib")


#if NTDDI_VERSION >= NTDDI_WIN8
constexpr DWORD WSA_FLAGS = WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED;
#else
constexpr DWORD WSA_FLAGS = WSA_FLAG_OVERLAPPED;
#endif


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

	//	获取扩展函数指针
	GUID guid = { 0x25a207b9, 0xddf3, 0x4660, { 0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e } };
	DWORD bytes = 0;
	void* ptr = nullptr;
	if (SOCKET_ERROR == ::WSAIoctl(_socket,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guid,
								   sizeof(guid),
								   &ptr,
								   sizeof(ptr),
								   &bytes,
								   nullptr,
								   nullptr))
	{
		std::cerr << "NetWork::Scoket::Scoket => WSAIoctl() failed! error : "
				  << ::WSAGetLastError() << std::endl;
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
		throw std::system_error(std::error_code(static_cast<int>(::WSAGetLastError()),
								std::system_category()), "NetWork::Scoket::Scoket => WSAIoctl() failed!");
	}
}

NetWork::Socket::~Socket()
{
	if (INVALID_SOCKET != _socket)
	{
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
}

NetWork::Service::Service()
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

bool NetWork::Service::associate_handle(HANDLE handle, ULONG_PTR key) noexcept
{
	HANDLE ret = ::CreateIoCompletionPort(handle, _iocp, key, 0);
	if (ret != _iocp)
	{
		std::cerr << "NetWork::Service::associate_handle => CreateIoCompletionPort() failed! error : "
				  << ::GetLastError() << std::endl;
		return false;
	}
	return true;
}

void NetWork::Test()
{
	std::cout << " ===== NetWork Bgein =====" << std::endl;

	std::cout << "WriteOperation sizeof :" << sizeof(WriteOperation) << std::endl;
	std::cout << "Scoket sizeof :" << sizeof(Socket) << std::endl;
	std::cout << "Service sizeof :" << sizeof(Service) << std::endl;

	std::cout << " ===== NetWork End =====" << std::endl;
}
