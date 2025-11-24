#pragma once

#include <print>
#include <queue>
#include "Observer.h"
#include "ringbuffer.h"
#include "concurrentqueue.h"


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

		void push(const T& value)
		{
			std::unique_lock lock(_mutex);
			_q.push(value);
		}

		void push(T&& value)
		{
			std::unique_lock lock(_mutex);
			_q.push(std::move(value));
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

	template<typename T>
	class MPSCQueueNonIntrusive
	{
	public:
		MPSCQueueNonIntrusive() : _head(new Node()), _tail(_head.load(std::memory_order_relaxed))
		{
			Node* front = _head.load(std::memory_order_relaxed);
			front->Next.store(nullptr, std::memory_order_relaxed);
		}

		~MPSCQueueNonIntrusive()
		{
			T* output;
			while (Dequeue(output))
				delete output;

			Node* front = _head.load(std::memory_order_relaxed);
			delete front;
		}

		void Enqueue(T* input)
		{
			Node* node = new Node(input);
			Node* prevHead = _head.exchange(node, std::memory_order_acq_rel);
			prevHead->Next.store(node, std::memory_order_release);
		}

		bool Dequeue(T*& result)
		{
			Node* tail = _tail.load(std::memory_order_relaxed);
			Node* next = tail->Next.load(std::memory_order_acquire);
			if (!next)
				return false;

			result = next->Data;
			_tail.store(next, std::memory_order_release);
			delete tail;
			return true;
		}

	private:
		struct Node
		{
			Node() = default;
			explicit Node(T* data) : Data(data)
			{
				Next.store(nullptr, std::memory_order_relaxed);
			}

			T* Data;
			std::atomic<Node*> Next;
		};

		std::atomic<Node*> _head;
		std::atomic<Node*> _tail;

		MPSCQueueNonIntrusive(MPSCQueueNonIntrusive const&) = delete;
		MPSCQueueNonIntrusive& operator=(MPSCQueueNonIntrusive const&) = delete;
	};

	template<typename T, std::atomic<T*> T::* IntrusiveLink>
	class MPSCQueueIntrusive
	{
		using Atomic = std::atomic<T*>;

	public:
		MPSCQueueIntrusive() : _dummy(), _dummyPtr(reinterpret_cast<T*>(_dummy.data())), _head(_dummyPtr), _tail(_dummyPtr)
		{
			// _dummy is constructed from raw byte array and is intentionally left uninitialized (it might not be default constructible)
			// so we init only its IntrusiveLink here
			Atomic* dummyNext = new (&(_dummyPtr->*IntrusiveLink)) Atomic();
			dummyNext->store(nullptr, std::memory_order_relaxed);
		}

		~MPSCQueueIntrusive()
		{
			T* output;
			while (Dequeue(output))
				delete output;

			// destruct our dummy atomic
			(_dummyPtr->*IntrusiveLink).~Atomic();
		}

		void Enqueue(T* input)
		{
			(input->*IntrusiveLink).store(nullptr, std::memory_order_release);
			T* prevHead = _head.exchange(input, std::memory_order_acq_rel);
			(prevHead->*IntrusiveLink).store(input, std::memory_order_release);
		}

		bool Dequeue(T*& result)
		{
			T* tail = _tail.load(std::memory_order_relaxed);
			T* next = (tail->*IntrusiveLink).load(std::memory_order_acquire);
			if (tail == _dummyPtr)
			{
				if (!next)
					return false;

				_tail.store(next, std::memory_order_release);
				tail = next;
				next = (next->*IntrusiveLink).load(std::memory_order_acquire);
			}

			if (next)
			{
				_tail.store(next, std::memory_order_release);
				result = tail;
				return true;
			}

			T* head = _head.load(std::memory_order_acquire);
			if (tail != head)
				return false;

			Enqueue(_dummyPtr);
			next = (tail->*IntrusiveLink).load(std::memory_order_acquire);
			if (next)
			{
				_tail.store(next, std::memory_order_release);
				result = tail;
				return true;
			}
			return false;
		}

	private:
		alignas(T) std::array<std::byte, sizeof(T)> _dummy;
		T* _dummyPtr;
		Atomic _head;
		Atomic _tail;

		MPSCQueueIntrusive(MPSCQueueIntrusive const&) = delete;
		MPSCQueueIntrusive& operator=(MPSCQueueIntrusive const&) = delete;
	};


	//	人总是看不上最容易得到的
	template<class T>
	using MutexQueue = LockQueue<T>;

	//	也许好一些，不够稳定		!!（spinlock 有BUG）
	template<class T>
	using SpinQueue = LockQueue<T, MS_Lock::Spinlock>;

	//	无锁 环形队列 一写一读
	template<class T>
	using SPSCQueue = RingBuffer::SPSCRingBuffer<T>;

	//	无锁 链式 多写一读
	template<typename T, std::atomic<T*> T::* IntrusiveLink = nullptr>
	using MPSCQueue = std::conditional_t<IntrusiveLink != nullptr, MPSCQueueIntrusive<T, IntrusiveLink>, MPSCQueueNonIntrusive<T>>;

	//	无锁 环形队列 多写多读，遵循FIFO原则   !!(错误的实现)
	template<class T>
	using MPMCQueue = RingBuffer::MPMCRingBuffer<T>;

	//	无锁 块+环形队列 多写多读，遵循FIFO原则。
	//	使用示列 https://github.com/cameron314/concurrentqueue/blob/master/samples.md
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

		constexpr uint32_t test_thread_count = 4;
		constexpr uint32_t test_count = 65536;
		constexpr uint32_t test_total = test_thread_count * test_count;

		std::atomic_uint_fast64_t read_count{ 0 };

		std::vector<ThreadGuardJoin> producers;
		std::vector<ThreadGuardJoin> consumers;
		producers.reserve(test_thread_count);
		consumers.reserve(test_thread_count);

		{
			ConcurrentQueue<node> con_que;
			con_que.enqueue({ 33, "ConcurrentQueue" });
			con_que.try_dequeue(item);

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
			read_count = 0;
			producers.clear();
			consumers.clear();

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

			std::this_thread::sleep_for(1s);
			std::print("MutexQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}

		{
			//	spinlock有BUG  
			/*
			SpinQueue<node> spin_que;
			read_count = 0;
			producers.clear();
			consumers.clear();

			auto start_time = std::chrono::high_resolution_clock::now();
			auto end_time = start_time;
			for (size_t i = 0; i < test_thread_count; i++)
			{
				producers.emplace_back(std::thread([&spin_que]() {
					for (int n = 0; n < test_count;) {
						spin_que.push({ n, std::format("http://example.com/{}", n).c_str() });
						++n;
					}
					}));
			}

			for (size_t i = 0; i < test_thread_count; i++)
			{
				consumers.emplace_back(std::thread([&spin_que, &end_time, &test_total, &read_count]() {
					node el;
					for (;;) {
						if (spin_que.pop(el)) {
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
			std::print("SpinQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
				std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
			*/
		}

		{
			//	MPMCQueue 错误的实现

			//MPMCQueue<node> mpmc_que(128);
			//read_count = 0;
			//producers.clear();
			//consumers.clear();

			//auto start_time = std::chrono::high_resolution_clock::now();
			//auto end_time = start_time;
			//for (size_t i = 0; i < test_thread_count; i++)
			//{
			//	producers.emplace_back(std::thread([&mpmc_que]() {
			//		for (int n = 0; n < 256;) {
			//			mpmc_que.write(n, std::format("http://example.com/{}", n).c_str() );
			//			++n;
			//		}
			//		}));
			//}

			//for (size_t i = 0; i < test_thread_count; i++)
			//{
			//	consumers.emplace_back(std::thread([&mpmc_que, &end_time, &test_total, &read_count]() {
			//		node el;
			//		for (;;) {
			//			if (mpmc_que.read(el)) {
			//				++read_count;
			//			}
			//			else {
			//				if (1024 == read_count) {
			//					end_time = std::chrono::high_resolution_clock::now();
			//					break;
			//				}
			//			}
			//		}
			//		}));
			//}

			//std::this_thread::sleep_for(1s);
			//std::print("MPMCQueue test {} count, thread num {}, use {}ms.\n", test_total, test_thread_count,
			//	std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
		}


		std::print(" ===== STL_Queue End =====\n");
	}
};
