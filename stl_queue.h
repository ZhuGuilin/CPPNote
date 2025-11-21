#pragma once

#include <print>
#include <queue>
#include "Observer.h"
#include "ringbuffer.h"
#include "concurrentqueue.h"
#include "SpinLock.h"

class STL_Queue : public Observer
{
public:

	template<class T, class LockType = std::mutex>
	class LockQueue
	{
	public:

		LockQueue() = default;
		~LockQueue() = default;

		LockQueue(LockQueue const&) = delete;
		LockQueue& operator=(LockQueue const&) = delete;

		void push(const T& o)
		{
			std::unique_lock lock(_mutex);
			_q.push(o);
		}

		void push(T&& o)
		{
			std::unique_lock lock(_mutex);
			_q.push(std::move(o));
		}

		template<typename... Args>
		void emplace(Args&&... args)
		{
			std::unique_lock lock(_mutex);
			_q.emplace(std::forward<Args>(args)...);
		}

		bool pop(T& o)
		{
			std::unique_lock lock(_mutex);
			if (_q.empty()) return false;
			o = std::move(_q.front());
			_q.pop();
			return true;
		}

		std::size_t size() const noexcept
		{
			std::unique_lock lock(_mutex);
			return _q.size();
		}

		bool empty() const noexcept
		{
			std::unique_lock lock(_mutex);
			return _q.empty();
		}

	private:

		std::queue<T> _q;
		mutable LockType _mutex;
	};

	template<class T>
	using MutexQueue = LockQueue<T>;

	template<class T>
	using SpinQueue = LockQueue<T, MS_Lock::Spinlock>;

	template<class T>
	using SPSCQueue = RingBuffer::SPSCRingBuffer<T>;

	template<class T>
	using MPMCQueue = RingBuffer::MPMCRingBuffer<T>;

	template<class T>
	using ConcurrentQueue = moodycamel::ConcurrentQueue<T>;

	struct node
	{
		int data;
		char str[32];

		node() = default;
		node(int v, const char* s)
			: data(v)
		{
			std::memcpy(str, s, sizeof(str) - 1);
		}
	};

	void Test() override
	{
		std::print(" ===== STL_Queue Bgein =====\n");
		using namespace std::chrono_literals;

		//	std::queue 先进先出队列
		std::queue<node> node_que;
		node_que.push({ 1, ""});
		node_que.push({ 9, ""});
		node_que.push({ 0, ""});
		auto& v = node_que.front();
		v.data += 10;
		node_que.pop();

		MutexQueue<node> mutexQ;
		node item;
		mutexQ.emplace(23, "haloua");
		mutexQ.push({ 25, "heihei" });
		auto ret = mutexQ.pop(item);
		ret = mutexQ.pop(item);
		ret = mutexQ.pop(item);

		{
			ConcurrentQueue<node> con_que;
			con_que.enqueue({ 33, "ConcurrentQueue" });
			con_que.try_dequeue(item);

			constexpr uint32_t test_thread_count = 4;
			constexpr uint32_t test_count = 65536;
			constexpr uint32_t test_total = test_thread_count * test_count;
			std::atomic_uint_fast64_t read_count{ 0 };

			std::vector<ThreadGuardJoin> producers;
			std::vector<ThreadGuardJoin> consumers;
			producers.reserve(test_thread_count);
			consumers.reserve(test_thread_count);

			auto start_time = std::chrono::high_resolution_clock::now();
			auto end_time = start_time;
			for (size_t i = 0; i < test_thread_count; i++)
			{
				producers.emplace_back(std::thread([&con_que]() {
					for (int n = 0; n < test_count;) {
						if (con_que.enqueue({ n, std::format("http://example.com/{}", n).c_str() })) {
							++n;
						}
						else {
							std::println("enqueue faild.");
						}
					}
					}));
			}

			for (size_t i = 0; i < test_thread_count; i++)
			{
				consumers.emplace_back(std::thread([&con_que, &end_time, &test_total, &read_count]() {
					node el;
					for (;;) {
						if (con_que.try_dequeue(el)) {
							++read_count;
						}
						else {
							if (test_total == read_count) {
								end_time = std::chrono::high_resolution_clock::now();
								break;
							}
						}
					}
					}));
			}

			std::this_thread::sleep_for(1s);
			std::print("ConcurrentQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}

		{
			MutexQueue<node> mutex_que;
			constexpr uint32_t test_thread_count = 4;
			constexpr uint32_t test_count = 65536;
			constexpr uint32_t test_total = test_thread_count * test_count;
			std::atomic_uint_fast64_t read_count{ 0 };

			std::vector<ThreadGuardJoin> producers;
			std::vector<ThreadGuardJoin> consumers;
			producers.reserve(test_thread_count);
			consumers.reserve(test_thread_count);

			auto start_time = std::chrono::high_resolution_clock::now();
			auto end_time = start_time;
			for (size_t i = 0; i < test_thread_count; i++)
			{
				producers.emplace_back(std::thread([&mutex_que]() {
					for (int n = 0; n < test_count;) {
						mutex_que.push({ n, std::format("http://example.com/{}", n).c_str() });
						++n;
					}
					}));
			}

			for (size_t i = 0; i < test_thread_count; i++)
			{
				consumers.emplace_back(std::thread([&mutex_que, &end_time, &test_total, &read_count]() {
					node el;
					for (;;) {
						if (mutex_que.pop(el)) {
							++read_count;
						}
						else {
							if (test_total == read_count) {
								end_time = std::chrono::high_resolution_clock::now();
								break;
							}
						}
					}
					}));
			}

			std::this_thread::sleep_for(2s);
			std::print("MutexQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}

		{
			SpinQueue<node> mutex_que;
			constexpr uint32_t test_thread_count = 4;
			constexpr uint32_t test_count = 65536;
			constexpr uint32_t test_total = test_thread_count * test_count;
			std::atomic_uint_fast64_t read_count{ 0 };

			std::vector<ThreadGuardJoin> producers;
			std::vector<ThreadGuardJoin> consumers;
			producers.reserve(test_thread_count);
			consumers.reserve(test_thread_count);

			auto start_time = std::chrono::high_resolution_clock::now();
			auto end_time = start_time;
			for (size_t i = 0; i < test_thread_count; i++)
			{
				producers.emplace_back(std::thread([&mutex_que]() {
					for (int n = 0; n < test_count;) {
						mutex_que.push({ n, std::format("http://example.com/{}", n).c_str() });
						++n;
					}
					}));
			}

			for (size_t i = 0; i < test_thread_count; i++)
			{
				consumers.emplace_back(std::thread([&mutex_que, &end_time, &test_total, &read_count]() {
					node el;
					for (;;) {
						if (mutex_que.pop(el)) {
							++read_count;
						}
						else {
							if (test_total == read_count) {
								end_time = std::chrono::high_resolution_clock::now();
								break;
							}
						}
					}
					}));
			}

			std::this_thread::sleep_for(2s);
			std::print("SpinQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}


		std::print(" ===== STL_Queue End =====\n");
	}
};
