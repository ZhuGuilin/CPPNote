#pragma once

#include <functional>
#include <handleapi.h>
#include <minwinbase.h>

#include "Observer.h"


class NetWork : public Observer
{
public:
	
	typedef struct
	{
		std::uint64_t len;
		std::int8_t*  buf;
	} MSWSABUF;

	struct MessageBuffer
	{
		typedef std::vector<std::uint8_t> BufferType;
		typedef std::vector<std::uint8_t>::size_type SizeType;

		MessageBuffer()
			: _storage(4096)
		{
		}

		explicit MessageBuffer(SizeType size)
			: _storage(size)
		{
		}

		MessageBuffer(MessageBuffer&& right) noexcept 
			: _wpos(right._wpos)
			, _rpos(right._rpos)
			, _storage(std::move(right).transfer())
		{
		}

		MessageBuffer& operator=(MessageBuffer&& right) noexcept
		{
			if (this != &right)
			{
				_wpos = right._wpos;
				_rpos = right._rpos;
				_storage = std::move(right).transfer();
			}

			return *this;
		}

		MessageBuffer(MessageBuffer const&) = default;
		MessageBuffer& operator=(MessageBuffer const&) = default;
		~MessageBuffer() = default;

		inline std::uint8_t* data() noexcept { return _storage.data(); }
		inline const std::uint8_t* data() const noexcept { return _storage.data(); }
		inline std::uint8_t* write_ptr() noexcept { return _storage.data() + _wpos; }
		inline const std::uint8_t* write_ptr() const noexcept { return _storage.data() + _wpos; }
		inline std::uint8_t* read_ptr() noexcept { return _storage.data() + _rpos; }
		inline const std::uint8_t* read_ptr() const noexcept { return _storage.data() + _rpos; }
		inline std::size_t size() const noexcept { return _storage.size(); }
		inline void reset() noexcept { _rpos = 0; _wpos = 0; }
		inline void resize(const SizeType size) noexcept { _storage.resize(size); }
		inline SizeType write_size() const noexcept { return _storage.size() - _wpos; }
		inline SizeType read_size() const noexcept { return _wpos - _rpos; }

		void write(const void* data, const SizeType size) noexcept
		{
			if (write_size() < size)
				_storage.resize(_wpos + size);

			std::memcpy(_storage.data() + _wpos, data, size);
			_wpos += size;
		}

		SizeType read(void* data, SizeType size) noexcept
		{
			if (read_size() < size)
				size = read_size();

			std::memcpy(data, _storage.data() + _rpos, size);
			_rpos += size;
			return size;
		}

		[[nodiscard]]
		BufferType&& transfer()
		{
			reset();
			return std::move(_storage);
		}

	private:

		SizeType	_rpos{ 0 };
		SizeType	_wpos{ 0 };
		BufferType  _storage;
	};

	class TcpSocket;
	struct Operation
	{
		typedef std::function<void(std::error_code, std::size_t)> CompletionHandler;

		OVERLAPPED	overlapped;
		CompletionHandler complete;
	};

	class address_v4
	{
	public:

		explicit address_v4(std::uint8_t addr[4]) noexcept;
		explicit address_v4(std::uint32_t addr) noexcept;
		explicit address_v4(std::string_view addr) noexcept;
		~address_v4() = default;

		address_v4(const address_v4& other) noexcept
			: _addr(other._addr)
		{
		}

		address_v4(address_v4&& other) noexcept
			: _addr(other._addr)
		{
		}

		address_v4& operator=(const address_v4& other) noexcept
		{
			_addr = other._addr;
			return *this;
		}

		address_v4& operator=(address_v4&& other) noexcept
		{
			_addr = other._addr;
			return *this;
		}

		inline static address_v4 any() noexcept
		{	//	0.0.0.0
			return address_v4(0x00u);
		}

		inline static address_v4 loopback() noexcept
		{	//	127.0.0.1
			return address_v4(0x7F000001u);
		}

		inline static address_v4 broadcast() noexcept
		{	//	255.255.255.255
			return address_v4(0xFFFFFFFFu);
		}
		
		inline std::uint32_t to_uint() const noexcept 
		{ 
			return ::ntohl(_addr.S_un.S_addr);
		}

		inline std::string to_string() const noexcept;

	private:

		in_addr _addr;
	};

	class Service;
	class Acceptor
	{
	public:

		explicit Acceptor(Service& service, const address_v4& addr, const std::uint32_t port);
		virtual ~Acceptor();

		void AsyncAccept();
		void shutdown() noexcept;

		inline bool isInvalid() const noexcept { return _listener == INVALID_SOCKET; }
		inline bool isOpen() const noexcept { return !_closed.load(); }

		Acceptor(const Acceptor&) = delete;
		Acceptor(Acceptor&&) = delete;
		Acceptor& operator=(const Acceptor&) = delete;
		Acceptor& operator=(Acceptor&&) = delete;

		void OnAcceptComplete(std::error_code ec, std::size_t size);

	private:

		bool ConfigureListeningSocket();
		bool Bind();


		SOCKET	_listener{ INVALID_SOCKET };
		void*	_lpfnAcceptEx{ nullptr };

		address_v4 _addr;
		std::uint32_t _port{ 0 };

		Service& _service;
		std::unique_ptr<Operation> _opt;

		MessageBuffer _readBuffer;
		std::atomic<bool> _closed;
	};

	class TcpSocket : public std::enable_shared_from_this<TcpSocket>
	{
	public:

		explicit TcpSocket();
		virtual ~TcpSocket();

		void shutdown() noexcept;

		void AsyncConnect(address_v4&& addr, const std::uint16_t port,
			std::function<void(std::error_code)>&& complete);
		void AsyncRead();
		void AsyncSend();

		inline SOCKET handle() const noexcept { return _socket; }
		inline bool isOpen() const noexcept { return _state.load() == State_Closed; }

		TcpSocket(TcpSocket const& other) = delete;
		TcpSocket(TcpSocket&& other) = delete;
		TcpSocket& operator=(TcpSocket const& other) = delete;
		TcpSocket& operator=(TcpSocket&& other) = delete;

		void OnConnectComplete(std::error_code ec, std::size_t size);
		void OnReadComplete(std::error_code ec, std::size_t size);
		void OnSendComplete(std::error_code ec, std::size_t size);

	private:

		SOCKET	_socket{ INVALID_SOCKET };
		void* _lpfnConnectEx{ nullptr };
		
		std::unique_ptr<Operation> _readOpt;
		std::unique_ptr<Operation> _sendOpt;

		MSWSABUF _wsabuf{ 0, nullptr };
		address_v4	_remoteAddress;
		MessageBuffer _readBuffer;
		MessageBuffer _sendBuffer;

		static constexpr std::uint8_t State_Open = 0x0;
		static constexpr std::uint8_t State_Closing = 0x1;
		static constexpr std::uint8_t State_Closed = 0x2;
		std::atomic<std::uint8_t> _state;
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

		HANDLE _iocp{ INVALID_HANDLE_VALUE };	// IOCP ¾ä±ú
		std::atomic<bool> _stopped;
	};

	void Test() override;
};