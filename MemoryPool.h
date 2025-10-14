#pragma once

#include <iostream>
#include <memory>

#include "SpinLock.h"
#include "Observer.h"

#define USE_MEMORY_POOL 1
#define SPIN_LOCK 1

#if SPIN_LOCK
typedef MS_Lock::Spinlock LockType;
#else
typedef std::mutex LockType;
#endif

constexpr uint32_t BLOCK_SIZE = 128;

class MemoryPool : public Observer
{
public:

	typedef unsigned char Byte;

	template <class T, uint32_t N = BLOCK_SIZE>
	class SimplePool
	{
	public:
		
		SimplePool() : _head(nullptr)
		{
			if (!expand())
			{
				std::cerr << "MemoryPool::SimplePool::expand() failed!" << std::endl;
				throw std::bad_alloc();
			}
		}

		~SimplePool()
		{
			std::lock_guard<LockType> lock(_lock);
			for (const auto& block : _blocks) 
			{
				::operator delete(block, std::align_val_t{ alignof(MemoryNode) });
			}
		}

		template<typename... Args>
		T* malloc(Args&&... args)
		{
			std::lock_guard<LockType> lock(_lock);
			if (nullptr == _head && !expand()) {
				std::cerr << "MemoryPool::SimplePool::malloc() failed!" << std::endl;
				return nullptr;
			}

			MemoryNode* node = _head;
			try {
				T* p = new (node->data) T(std::forward<Args>(args)...);
				_head = _head->next;
				return p;
			}
			catch (...) {
				return nullptr;
			}
		}

		void free(T* p) noexcept
		{
			std::lock_guard<LockType> lock(_lock);
			MemoryNode* node = reinterpret_cast<MemoryNode*>(reinterpret_cast<Byte*>(p) - offsetof(MemoryNode, data));
			reinterpret_cast<T*>(node->data)->~T();
			node->next = _head;
			_head = node;
		}

		SimplePool(const SimplePool&) = delete;
		SimplePool& operator=(const SimplePool&) = delete;

	private:

		struct MemoryNode
		{
			alignas(alignof(T)) Byte data[sizeof(T)];
			MemoryNode* next = nullptr;
		};
		static_assert(offsetof(SimplePool::MemoryNode, data) == 0, "MemoryNode.data must be the first member");

		inline bool expand()
		{
			const uint32_t size = max(BLOCK_SIZE, N);
			MemoryNode* block = static_cast<MemoryNode*>(::operator new(
				sizeof(MemoryNode) * size, std::align_val_t{ alignof(MemoryNode) }));
			if (nullptr == block)
				return false;

			_blocks.push_back(block);

			MemoryNode* node = block;
			for (uint32_t i = 1; i < size; i++)
			{
				node->next = block + i;
				node = node->next;
			}
			node->next = _head;
			_head = block;
			return true;
		}

		MemoryNode* _head;
		LockType _lock;
		std::vector<MemoryNode*> _blocks;
	};

	struct MyStruct
	{
		int		level;
		int		value;
		Byte	data[32];

		MyStruct() = default;
		MyStruct(int lv, int val) 
			: level(lv), value(val), data{0} 
		{
		}
		~MyStruct() {}
	};

	class ThreadGuard
	{
		std::thread _t;

	public:

		explicit ThreadGuard(std::thread&& t) : _t(std::move(t))
		{
			if (!_t.joinable())
				throw std::logic_error("no thread!");
		}

		virtual ~ThreadGuard()
		{
			if (_t.joinable())
				_t.join();
		}

		ThreadGuard(const ThreadGuard&) = delete;
		ThreadGuard(ThreadGuard&&) = default;
		ThreadGuard& operator=(const ThreadGuard&) = delete;
		ThreadGuard& operator=(ThreadGuard&&) = delete;
	};
#pragma optimize("", off)
	void Test() override
	{
		std::cout << " ===== MemoryPool Bgein =====" << std::endl;
		
		SimplePool<MyStruct> pool;
		std::cout << "sizeof std::string :" << sizeof(std::string) << std::endl;
		std::cout << "sizeof MyStruct :	" << sizeof(MyStruct) << std::endl;
		std::cout << "alignof MyStruct :" << alignof(MyStruct) << std::endl;
		
		std::size_t total{ 0 };
		auto start = std::chrono::high_resolution_clock::now();
		{
			const int T_NUM = 4;
			std::vector<ThreadGuard> works;
			works.reserve(T_NUM);
			for (int i = 0; i < T_NUM; i++)
			{
				works.emplace_back(std::thread([&pool, &total,i]()
				{
					for (int j = 0; j < 2000000; j++)
					{
#if USE_MEMORY_POOL
						MyStruct* p = pool.malloc(i, j);
						if (nullptr == p)
						{
							std::cerr << "malloc failed!" << std::endl;
							return;
						}
						total += (p->value & 0xffff);
						pool.free(p);
#else
						MyStruct* p = new MyStruct(i, j);
						if (nullptr == p)
						{
							std::cerr << "malloc failed!" << std::endl;
							return;
						}
						total += (p->value & 0xffff);
						delete p;

#endif
					}
				}));
			}
		}

		auto end = std::chrono::high_resolution_clock::now();
#if USE_MEMORY_POOL
		std::cout << "MemoryPool 内存池测试 共耗时: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			<< " ms" << "Total : " << total << std::endl;
#else
		std::cout << "::new 共耗时: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			<< " ms" << "Total : " << total << std::endl;
#endif

		std::cout << " ===== MemoryPool End =====" << std::endl;
	}
#pragma optimize("", on)
};
