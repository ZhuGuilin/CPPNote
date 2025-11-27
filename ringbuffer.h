#pragma once

#include <print>
#include <assert.h>

#include "observer.h"
#include "stl_thread.h"
#include "SpinLock.h"


class RingBuffer : public Observer
{
public:

	//	单生产单消费环形缓冲区，1写1读队列
	template<class T>
	class SPSCRingBuffer
	{
	public:

		explicit SPSCRingBuffer(uint32_t capacity)
			: _size(NextPowerOfTwo(capacity))
			, _mask(_size - 1)
			, _storage(static_cast<T*>(std::malloc(sizeof(T) * _size)))
			, _rpos(0)
			, _wpos(0)
		{
			static_assert(std::is_nothrow_destructible<T>::value,
				"SPSCRingBuffer requires a nothrow destructible type");
			assert(_size >= 2 && "SPSCRingBuffer size must be at least 2");
			if (_storage == nullptr)
			{
				throw std::bad_alloc();
			}
		}

		~SPSCRingBuffer()
		{
			uint32_t rpos = _rpos.load(std::memory_order_relaxed);
			const uint32_t wpos = _wpos.load(std::memory_order_relaxed);
			while (rpos != wpos)
			{
				_storage[rpos & _mask].~T();
				++rpos;
			}

			std::free(_storage);
		}

		bool empty() const noexcept
		{
			return _rpos.load(std::memory_order_acquire) == _wpos.load(std::memory_order_acquire);
		}

		bool full() const noexcept
		{
			return (_wpos.load(std::memory_order_acquire) + 1) == _rpos.load(std::memory_order_acquire);
		}

		uint32_t capacity() const noexcept
		{ 
			return _size - 1;
		}

		template<typename... Args>
		bool write(Args&&... args)
		{
			const uint32_t wpos = _wpos.load(std::memory_order_relaxed);
			const uint32_t next_wpos = wpos + 1;
			if (next_wpos - _rpos.load(std::memory_order_acquire) >= _size) {
				return false; // full
			}

			new (&_storage[wpos & _mask]) T(std::forward<Args>(args)...);
			_wpos.store(next_wpos, std::memory_order_release);
			return true;
		}

		template<typename Iterator>
		uint32_t write_bulk(Iterator begin, uint32_t count)
		{
			const uint32_t wpos = _wpos.load(std::memory_order_relaxed);
			const uint32_t rpos = _rpos.load(std::memory_order_acquire);
			const uint32_t cap = capacity() - (wpos - rpos);
			const uint32_t num = std::min(count, cap);
			for (uint32_t i = 0; i < num; ++i) {
				new (&_storage[(wpos + i) & _mask]) T(std::move(*begin++));
			}

			_wpos.store(wpos + num, std::memory_order_release);
			return num;
		}

		template<typename Iterator>
		uint32_t write_bulk(Iterator begin, Iterator end)
		{
			return write_bulk(begin, static_cast<uint32_t>(std::distance(begin, end)));
		}

		bool read(T& item)
		{
			const uint32_t rpos = _rpos.load(std::memory_order_relaxed);
			if (rpos == _wpos.load(std::memory_order_acquire)) {
				return false; // empty
			}

			item = std::move(_storage[rpos & _mask]);
			_storage[rpos & _mask].~T();
			_rpos.store(rpos + 1, std::memory_order_release);
			return true;
		}

		template<typename Iterator>
		uint32_t read_bulk(Iterator begin, uint32_t count)
		{
			const uint32_t rpos = _rpos.load(std::memory_order_relaxed);
			const uint32_t wpos = _wpos.load(std::memory_order_acquire);
			const uint32_t has = wpos - rpos;
			const uint32_t num = std::min(count, has);
			for (uint32_t i = 0; i < num; ++i) {
				*begin++ = std::move(_storage[(rpos + i) & _mask]);
				_storage[(rpos + i) & _mask].~T();
			}

			_rpos.store(rpos + num, std::memory_order_release);
			return num;
		}

		SPSCRingBuffer(SPSCRingBuffer const&) = delete;
		SPSCRingBuffer& operator=(SPSCRingBuffer const&) = delete;
		SPSCRingBuffer& operator=(SPSCRingBuffer&& rhs) = delete;

	private:

		static uint32_t NextPowerOfTwo(uint32_t n) noexcept
		{
			if (n == 0) return 1;

			n--;
			n |= n >> 1;
			n |= n >> 2;
			n |= n >> 4;
			n |= n >> 8;
			n |= n >> 16;
			return n + 1;
		}

		static constexpr std::size_t kCacheLine = 64;

		alignas(kCacheLine) std::atomic<uint32_t> _wpos;
		alignas(kCacheLine) std::atomic<uint32_t> _rpos;

		const uint32_t _size;
		const uint32_t _mask;
		T* const _storage;
	};

	struct DefultTraits
	{
		static constexpr uint32_t kSpinCutoff = 2000;	//	自选等待测试次数
		static constexpr std::chrono::nanoseconds kSleepNs{ 200 };		//	睡眠时间
	};

	struct Traits1 : public DefultTraits
	{
		//	自定义
		static constexpr uint32_t kSpinCutoff = 5000;
		static constexpr std::chrono::nanoseconds kSleepNs{ 200 };
	};

	//	多生产多消费环形缓冲区
	//	实现参考folly::MPMCQueue，核心思路由整个队列的竞争 -> 单个槽位的竞争
	template<class T, typename Traits = DefultTraits>
	class MPMCRingBuffer
	{
	public:

		explicit MPMCRingBuffer(uint32_t capacity = 2048)
			: _size(NextPowerOfTwo(capacity))
			, _mask(_size - 1)
			, _storage(static_cast<T*>(std::malloc(sizeof(T)* _size)))
			, _wpos(0)
			, _rpos(0)
			, _wpre(0)
			, _rpre(0)
		{
			static_assert(std::is_nothrow_destructible<T>::value,
				"SPSCRingBuffer requires a nothrow destructible type");
			assert(_size >= 2 && "SPSCRingBuffer size must be at least 2");
			if (_storage == nullptr)
			{
				throw std::bad_alloc();
			}
		}

		~MPMCRingBuffer()
		{
			uint64_t rpos = _rpos.load(std::memory_order_relaxed);
			const uint64_t wpos = _wpos.load(std::memory_order_relaxed);
			while (rpos != wpos)
			{
				_storage[rpos & _mask].~T();
				++rpos;
			}

			std::free(_storage);
		}

		void write(const T& item)
		{
			//	预约一个可写的位置
			uint64_t ticket = _wpre++;
			uint32_t spinCount = 0;

			//	等待轮到可写
			while(_wpos.load(std::memory_order_relaxed) != ticket)
			{
				if (++spinCount > Traits::kSpinCutoff) {
					std::this_thread::sleep_for(Traits::kSleepNs);
				}
				else {
					asm_volatile_pause();
				}
			}

			new (&_storage[ticket & _mask]) T(item);
			_wpos.store(ticket + 1, std::memory_order_release);
		}

		void write(T&& item)
		{
			//	预约一个可写的位置
			uint64_t ticket = _wpre++;
			uint32_t spinCount = 0;

			//	等待轮到可写
			while (_wpos.load(std::memory_order_relaxed) != ticket)
			{
				if (++spinCount > Traits::kSpinCutoff) {
					std::this_thread::sleep_for(Traits::kSleepNs);
				}
				else {
					asm_volatile_pause();
				}
			}

			new (&_storage[ticket & _mask]) T(std::forward<T>(item));
			_wpos.store(ticket + 1, std::memory_order_release);
		}

		template<typename... Args>
		void write(Args&&... args)
		{
			//	预约一个可写的位置
			uint64_t ticket = _wpre++;
			uint32_t spinCount = 0;

			//std::print("thread id: {}, pre write ticket: {}.\n", std::this_thread::get_id(), ticket);
			//std::this_thread::sleep_for(1s);

			//	等待轮到可写
			while (_wpos.load(std::memory_order_acquire) != ticket)
			{
				//std::print("thread id: {}, waitting write [{} : {}].\n", 
				//	std::this_thread::get_id(), _wpos.load(std::memory_order_relaxed), ticket);
				if (++spinCount > Traits::kSpinCutoff) {
					std::this_thread::sleep_for(Traits::kSleepNs);
				}
				else {
					asm_volatile_pause();
				}
			}

			//std::print("thread id: {}, writting ticket: {}.\n", std::this_thread::get_id(), ticket);
			new (&_storage[ticket & _mask]) T(std::forward<Args>(args)...);
			_wpos.store(ticket + 1, std::memory_order_release);
		}

		bool read(T& item)
		{
			const uint64_t rpre = _rpre.load(std::memory_order_acquire);
			if (rpre == _wpos.load(std::memory_order_acquire)) {
				//std::print("thread id: {}, pre read faild, pre is full: {}.\n", std::this_thread::get_id(), rpre);
				return false; // 预约满了
			}

			// !!! 这里可能会有多个线程进来，而此时_wpos可能只比_repre大1 进而发生预约和读取不再受限制
			uint64_t ticket = _rpre++;
			uint32_t spinCount = 0;
			//std::print("thread id: {}, pre read ticket: {}.\n", std::this_thread::get_id(), ticket);

			//	等待轮到可读
			while (_rpos.load(std::memory_order_acquire) != ticket)
			{
				//std::print("thread id: {}, waitting read [{} : {}].\n", 
				//	std::this_thread::get_id(), _rpos.load(std::memory_order_relaxed), ticket);
				if (++spinCount > Traits::kSpinCutoff) {
					std::this_thread::sleep_for(Traits::kSleepNs);
				}
				else {
					asm_volatile_pause();
				}
			}

			item = std::move(_storage[ticket & _mask]);
			_storage[ticket & _mask].~T();
			_rpos.store(ticket + 1, std::memory_order_release);
			//std::print("thread id: {}, reading ticket: {} data: {}.\n", std::this_thread::get_id(), ticket, item.data);
			return true;
		}

		MPMCRingBuffer(MPMCRingBuffer const&) = delete;
		MPMCRingBuffer& operator=(MPMCRingBuffer const&) = delete;
		MPMCRingBuffer& operator=(MPMCRingBuffer&& rhs) = delete;

	private:

		static uint32_t NextPowerOfTwo(uint32_t n) noexcept
		{
			if (n == 0) return 1;

			n--;
			n |= n >> 1;
			n |= n >> 2;
			n |= n >> 4;
			n |= n >> 8;
			n |= n >> 16;
			return n + 1;
		}

		static constexpr std::size_t kCacheLine = 64;

		//	真实的读写位置
		alignas(kCacheLine) std::atomic<uint64_t> _wpos;
		alignas(kCacheLine) std::atomic<uint64_t> _rpos;

		//	预约员，记录预约出去的位置
		//  写预约不能超过实际可读位置，读预约不能超过实际可写位置
		alignas(kCacheLine) std::atomic<uint64_t> _wpre;
		alignas(kCacheLine) std::atomic<uint64_t> _rpre;

		const uint64_t _size;
		const uint64_t _mask;
		T* const _storage;
	};

	struct TestData
	{
		int index;
		uint32_t data;
		char url[32];
		void* ptr;

		TestData()
			: index(0)
			, data(0)
			, ptr(nullptr)
		{
			std::memset(url, 0, sizeof(url));
			//std::print("base construct.\n");
		}

		explicit TestData(int idx, uint32_t d, const char* u, void* p) noexcept
			: index(idx)
			, data(d)
			, ptr(p)
		{
			std::memcpy(url, u, sizeof(url) - 1);
			//std::print("construct TestData index : {}.\n", index);
		}

		~TestData() noexcept
		{
			//std::print("destruct ~TestData index : {}.\n", index);
		}

		TestData(TestData const&) = delete;
		TestData& operator=(TestData const&) = delete;

		TestData(TestData&& o)
		{
			index = o.index;
			data = o.data;
			ptr = o.ptr;
			std::memcpy(url, o.url, sizeof(url) - 1);
			//std::print("move construct index : {}.\n", index);
		}

		TestData& operator=(TestData&& o)
		{
			index = o.index;
			data = o.data;
			ptr = o.ptr;
			std::memcpy(url, o.url, sizeof(url) - 1);
			return *this;
			//std::print("move operator= index : {}.\n", index);
		}
	};
	void Test() override
	{
		std::print(" ===== RingBuffer Bgein =====\n");
		using namespace std::chrono_literals;
		
		{
			SPSCRingBuffer<TestData> ringbuf(25);
			TestData item;

			auto em = ringbuf.empty();
			ringbuf.write(TestData{ 1, 100, "http://example.com/1", nullptr });
			ringbuf.write(2, 101, "http://example.com/2", &ringbuf);
			em = ringbuf.empty();

			ringbuf.read(item);
			ringbuf.read(item);
			em = ringbuf.empty();

			std::vector<TestData> vecIn, vecOut;
			for (int i = 0; i < 35; ++i)
			{
				vecIn.emplace_back(i, i * 10 + 1, "", nullptr);
			}

			ringbuf.write_bulk(vecIn.begin(), vecIn.end());
			ringbuf.read(item);
			//vecOut.reserve(8);	//	reserve只开辟空间，不构造元素
			vecOut.resize(8);
			ringbuf.read_bulk(vecOut.begin(), 4);
			ringbuf.read_bulk(vecOut.begin() + 4, 4);
		}

		{
			SPSCRingBuffer<TestData> buffer(1024);
			constexpr uint32_t test_count = 65536;
			
			auto start_time = std::chrono::high_resolution_clock::now();
			auto end_time = start_time;
			ThreadGuardJoin producer(std::thread([&buffer]() {
				for (int i = 0; i < test_count;)
				{
					if (buffer.write(i, i, std::format("http://example.com/{}", i).c_str(), nullptr)) {
						++i;
					}
					else {
						std::println("write faild.");
					}
				}
				}));

			ThreadGuardJoin consumer(std::thread([&buffer, &end_time]() {
				TestData item;
				for (int i = 0;;)
				{
					if (buffer.read(item)) {
						++i;
						if (test_count -1 == i) {
							end_time = std::chrono::high_resolution_clock::now();
							break;
						}
					}
				}
				}));

			std::this_thread::sleep_for(1s);
			std::print("spsc ringbuffer test write and read {} count, use {}ms.\n", test_count, 
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}

		std::print(" ===== RingBuffer End =====\n");
	}
};