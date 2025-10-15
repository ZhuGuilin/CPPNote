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
		Operation() noexcept
		{
			std::memset(static_cast<OVERLAPPED*>(this), 0, sizeof(OVERLAPPED));
		}

		virtual ~Operation() = default;

		virtual void Complete(const std::error_code& ec, std::size_t bytes) = 0;
	};

	struct ConnectOperation : public Operation 
	{
		std::function<void(const std::error_code)> handler;
		ConnectOperation(std::function<void(std::error_code)> h)
			: handler(std::move(h)) 
		{
		}

		void Complete(const std::error_code& ec, std::size_t bytes) noexcept override
		{
			if (handler) 
				handler(ec);
		}
	};

	struct ReadOperation : public Operation 
	{
		std::function<void(const std::error_code, size_t)> handler;
		ReadOperation(std::function<void(std::error_code, std::size_t)> h)
			: handler(std::move(h)) 
		{
		}

		void Complete(const std::error_code& ec, std::size_t bytes) noexcept override
		{
			if (handler) 
				handler(ec, bytes);
		}
	};

	struct WriteOperation : public Operation 
	{
		std::function<void(const std::error_code, size_t)> handler;
		WriteOperation(std::function<void(std::error_code, std::size_t)> h)
			: handler(std::move(h)) 
		{
		}

		void Complete(const std::error_code& ec, std::size_t bytes) override
		{
			if (handler) 
				handler(ec, bytes);
		}
	};

	class Service;
	class Socket
	{
	public:

		enum class Type { TCP, UDP };
		explicit Socket(Service& service, int af, Type type);
		explicit Socket(Service& service, SOCKET s, int af, Type type);
		~Socket();

		inline bool IsOpen() const noexcept { return _socket != SOCKET_ERROR; }
		inline bool IsLive() const noexcept { return _live; }

		void Bind(const std::string& ip, const uint16_t port);
		void Listen(int backlog = SOMAXCONN);
		std::unique_ptr<Socket> Accept();

		void AsyncConnect(const std::string& host, const uint16_t port,
			std::function<void(std::error_code)>&& handler);
		void AsyncRead(void* buffer, size_t size,
			std::function<void(std::error_code, size_t)>&& handler);
		void AsyncWrite(const void* data, size_t size,
			std::function<void(std::error_code, size_t)>&& handler);

		Socket(const Socket&) = delete;
		Socket(Socket&&) = default;
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) = default;

	private:

		SOCKET	 _socket{ INVALID_SOCKET };
		Type	 _type{ Type::TCP };	// 套接字类型 TCP/UDP
		Service& _service;				// 关联的服务对象

		bool     _live{ false };		// 启用状态

		std::unique_ptr<ConnectOperation> _connect_op;
		std::unique_ptr<ReadOperation> _read_op;
		std::unique_ptr<WriteOperation> _write_op;

		void* _connectEx{ nullptr };	// ConnectEx 函数指针
	};

	class Service
	{
	public:

		explicit Service();
		~Service();

		void Post(Operation* op) noexcept;
		bool RegisterHandle(HANDLE handle, ULONG_PTR key) noexcept;
		void Stop() noexcept;
		void run();

		HANDLE IocpHandle() const noexcept { return _iocp; }

		Service(const Service&) = delete;
		Service(Service&&) = delete;
		Service& operator=(const Service&) = delete;
		Service& operator=(Service&&) = delete;

	private:

		HANDLE _iocp{ INVALID_HANDLE_VALUE };	// IOCP 句柄
		std::atomic<bool> _stopped;
	};

	void Test() override;
};