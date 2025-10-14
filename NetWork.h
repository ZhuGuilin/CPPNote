#pragma once

#include <functional>
#include <handleapi.h>
#include <minwinbase.h>
#include "Observer.h"


class NetWork : public Observer
{
public:

	struct Operation : public OVERLAPPED 
	{
		using Handler = std::function<void(const std::error_code&, std::size_t)>;

		Operation(Handler&& handler) noexcept
			: _handler(std::move(handler))
		{
			std::memset(static_cast<OVERLAPPED*>(this), 0, sizeof(OVERLAPPED));
		}

		virtual ~Operation() = default;

		void Complete(const std::error_code& ec, std::size_t bytes)
		{
			if (_handler)
				_handler(ec, bytes);
		}

	private:

		Handler _handler;	//	完成操作后的回调 使用函数对象避免虚函数开销
	};

	struct ConnectOperation : public Operation 
	{
		ConnectOperation(Handler&& handler) noexcept
			: Operation(std::move(handler)) 
		{
		}
	};

	struct ReadOperation : public Operation 
	{
		ReadOperation(Handler&& handler) noexcept
			: Operation(std::move(handler)) 
		{
		}
	};

	struct WriteOperation : public Operation 
	{
		WriteOperation(Handler&& handler)
			: Operation(std::move(handler))
		{
		}
	};

	class Service;
	class Socket
	{
	public:

		enum class Type { TCP, UDP };
		explicit Socket(Service& service, int af, Type type);
		~Socket();

		Socket(const Socket&) = delete;
		Socket(Socket&&) = delete;
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) = delete;

	private:

		SOCKET	 _socket{ INVALID_SOCKET };
		Type	 _type{ Type::TCP };	// 套接字类型 TCP/UDP
		Service& _service;				// 关联的服务对象
	};

	class Service
	{
	public:

		explicit Service();
		~Service();

		HANDLE get_iocp() const noexcept { return _iocp; }
		bool associate_handle(HANDLE handle, ULONG_PTR key) noexcept;

		Service(const Service&) = delete;
		Service(Service&&) = delete;
		Service& operator=(const Service&) = delete;
		Service& operator=(Service&&) = delete;

	private:

		HANDLE _iocp{ INVALID_HANDLE_VALUE };	// IOCP 句柄
	};

	void Test() override;
};