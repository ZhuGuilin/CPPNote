#pragma once

#include <print>
#include <assert.h>

#include "observer.h"
#include "stl_thread.h"


class RingBuffer : public Observer
{
public:

	//	单生产单消费环形缓冲区，也可用作1写1读队列
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
			if ((next_wpos & _mask) == (_rpos.load(std::memory_order_acquire) & _mask)) {
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
			const uint32_t num = min(count, cap);
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
			const uint32_t num = min(count, has);
			for (uint32_t i = 0; i < num; ++i) {
				*begin++ = std::move(_storage[(rpos + i) & _mask]);
				_storage[(rpos + i) & _mask].~T();
			}

			_rpos.store(rpos + num, std::memory_order_release);
			return num;
		}

		SPSCRingBuffer(SPSCRingBuffer const&) = delete;
		SPSCRingBuffer& operator=(SPSCRingBuffer const&) = delete;

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

		using AtomicIndex = std::atomic<uint32_t>;
		static constexpr std::size_t kCacheLine = 64;

		alignas(kCacheLine) AtomicIndex _rpos;
		alignas(kCacheLine) AtomicIndex _wpos;

		const uint32_t _size;
		const uint32_t _mask;
		T* const _storage;
	};

	//	多生产多消费环形缓冲区，也可用作多写多读队列、1写多读和多写1读队列
	//	实现参考folly::MPMCQueue
	template<class T>
	class MPMCRingBuffer
	{

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
			//std::vector<TestData> reader;
			//reader.resize(test_count);
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
					else {
						//std::this_thread::yield();
					}
				}
				}));

			std::this_thread::sleep_for(2s);
			std::print("spsc ringbuffer test write and read {} count, use {}ms.\n", test_count, 
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}

		std::print(" ===== RingBuffer End =====\n");
	}
};