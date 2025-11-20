#pragma once

#include <print>
#include <queue>
#include "Observer.h"
#include "ringbuffer.h"

class STL_Queue : public Observer
{
public:

	template<class T>
	using SPSCQueue = RingBuffer::SPSCRingBuffer<T>;

	template<class T>
	using MPMCQueue = RingBuffer::MPMCRingBuffer<T>;

	template<class T>
	class MutexQueue
	{
	public:

		MutexQueue() = default;
		~MutexQueue() = default;

		MutexQueue(MutexQueue const&) = delete;
		MutexQueue& operator=(MutexQueue const&) = delete;

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
			if (empty()) {
				return false;
			}

			std::unique_lock lock(_mutex);
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
		mutable std::mutex _mutex;
	};

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

		std::print(" ===== STL_Queue End =====\n");
	}
};
