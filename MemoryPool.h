#pragma once

#include <print>
#include <memory>
#include <memory_resource>
#include <cassert>

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

	//	基于栈空间的内存池实现,用于小空间高速计算。
	//  与std::pmr::monotonic_buffer_resource类似. copy from trinitycore
	template <std::size_t N, std::size_t alignment = alignof(std::max_align_t)>
	class arena
	{
		alignas(alignment) char _buf[N];
		char* _ptr;

	public:

		~arena() { _ptr = nullptr; }
		arena() noexcept : _ptr(_buf) {}
		arena(const arena&) = delete;
		arena& operator=(const arena&) = delete;

		template <std::size_t ReqAlign> 
		char* allocate(std::size_t n)
		{
			static_assert(ReqAlign <= alignment, "alignment is too small for this arena");
			assert(pointer_in_buffer(_ptr) && "stack memory pool has outlived arena");
			auto const aligned_n = align_up(n);
			if (static_cast<decltype(aligned_n)>(_buf + N - _ptr) >= aligned_n)
			{
				char* r = _ptr;
				_ptr += aligned_n;
				return r;
			}

			static_assert(alignment <= alignof(std::max_align_t), "you've chosen an "
				"alignment that is larger than alignof(std::max_align_t), and "
				"cannot be guaranteed by normal operator new");
			return static_cast<char*>(::operator new(n));
		}

		void deallocate(char* p, std::size_t n) noexcept
		{
			assert(pointer_in_buffer(_ptr) && "short_alloc has outlived arena");
			if (pointer_in_buffer(p))
			{
				n = align_up(n);
				if (p + n == _ptr)
					_ptr = p;
			}
			else
			{
				::operator delete(p);
			}
		}

#if 1	//	调试用，查看起始buf地址
		auto constexpr buf_address() noexcept { return std::uintptr_t(_buf); }
#endif
		static constexpr std::size_t size() noexcept { return N; }
		std::size_t used() const noexcept { return static_cast<std::size_t>(_ptr - _buf); }
		void reset() noexcept { _ptr = _buf; }

	private:

		static std::size_t align_up(std::size_t n) noexcept
		{
			return (n + (alignment - 1)) & ~(alignment - 1);
		}

		bool pointer_in_buffer(char* p) noexcept
		{
			return std::uintptr_t(_buf) <= std::uintptr_t(p) &&
				std::uintptr_t(p) <= std::uintptr_t(_buf) + N;
		}
	};

	template <class T, std::size_t N, std::size_t Align = alignof(std::max_align_t)>
	struct StackPool
	{
		using value_type = T;
		static auto constexpr alignment = Align;
		static auto constexpr size = N;
		using arena_type = arena<size, alignment>;

		StackPool(const StackPool&) = default;
		StackPool& operator=(const StackPool&) = delete;

		StackPool(arena_type& a) noexcept 
			: _arena(a)
		{
			static_assert(size % alignment == 0, "size N needs to be a multiple of alignment Align");
		}

		template <class U>
		StackPool(const StackPool<U, N, alignment>& a) noexcept
			: _arena(a._arena)
		{
		}

		template <class _Up> 
		struct rebind { using other = StackPool<_Up, N, alignment>; };

		T* allocate(std::size_t n)
		{
			return reinterpret_cast<T*>(_arena.template allocate<alignof(T)>(n * sizeof(T)));
		}

		void deallocate(T* p, std::size_t n) noexcept
		{
			_arena.deallocate(reinterpret_cast<char*>(p), n * sizeof(T));
		}

		template <class T1, std::size_t N1, std::size_t A1>
		bool operator==(const StackPool<T1, N1, A1>& other) noexcept
		{
			return (N == N1) && (A1 == Align) && (&other._arena) == (&_arena);
		}

		template <class T1, std::size_t N1, std::size_t A1>
		bool operator!=(const StackPool<T1, N1, A1>& other) noexcept
		{
			return !(other == *this);
		}

		arena_type& _arena;
	};

	//	链表式简单内存池实现
	template <class T, uint32_t N = BLOCK_SIZE>
	class SimplePool
	{
	public:
		
		SimplePool() : _head(nullptr)
		{
			if (!expand())
			{
				std::print("MemoryPool::SimplePool::expand() failed!\n");
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
				std::print("MemoryPool::SimplePool::malloc() failed!\n");
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

	struct alignas(std::max_align_t) MyStruct
	{
		int		level;
		int		value;
		Byte	data[27];

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
		std::print(" ===== MemoryPool Bgein =====\n");

		//	使用栈空间分配内存
		std::print("alignof(std::max_align_t) : {}.\n", alignof(std::max_align_t));
		using allocator_type = StackPool<MyStruct, 
			(64 * sizeof(MyStruct) + (alignof(std::max_align_t) - 1)) & ~(alignof(std::max_align_t) - 1)>;
		using arena_type = typename allocator_type::arena_type;
		using stack_storage = std::vector<MyStruct, allocator_type>;
		arena_type arena;
		stack_storage stack_vec(arena);
		stack_vec.reserve(32);
		for (int i = 0; i < 4; i++)
		{
			stack_vec.emplace_back(i, i * 10);
		}

		std::print("sizeof std::vector   : {}.\n", sizeof std::vector<MyStruct>);
		std::print("sizeof stack_storage : {}.\n", sizeof stack_storage);
		std::print("stack_vec address  : {}.\n", std::uintptr_t(&stack_vec));
		std::print("arena address      : {}.\n", std::uintptr_t(&arena));
		std::print("arena buf address  : {}.\n", arena.buf_address());	//	与arena地址相同
		
		for (const auto& item : stack_vec)
		{
			//	成员地址确实在栈空间中分配
			std::print("stack item address : {}.\n", std::uintptr_t(&item));
		}

		//	使用 std::pmr 分配内存
		char buffer[64 * sizeof(MyStruct)];
		std::pmr::monotonic_buffer_resource pmr_pool(std::data(buffer), std::size(buffer));
		std::pmr::vector<MyStruct> pmr_vec(&pmr_pool);
		pmr_vec.reserve(32);
		for (int i = 0; i < 4; i++)
		{
			pmr_vec.emplace_back(i, i * 123);
		}

		std::print("pmr_vec address    : {}.\n", std::uintptr_t(&pmr_vec));
		for (const auto& item : pmr_vec)
		{
			//	成员地址确实在 buffer空间中分配
			std::print("pmr item address   : {}.\n", std::uintptr_t(&item));
		}
		
		//	使用链表内存池分配内存
		SimplePool<MyStruct> pool;
		std::print("sizeof std::string : {}.\n", sizeof(std::string));
		std::print("sizeof MyStruct : {}.\n", sizeof(MyStruct));
		std::print("alignof MyStruct : {}.\n", alignof(MyStruct));
		
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
							std::print("malloc failed! \n");
							return;
						}
						total += (p->value & 0xffff);
						pool.free(p);
#else
						MyStruct* p = new MyStruct(i, j);
						if (nullptr == p)
						{
							std::print("malloc failed! \n");
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
		std::print("MemoryPool 内存池测试 共耗时 : {}, Total : {}.\n",
			std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), total);
#else
		std::print("::new 共耗时 : {}, Total : {}.\n",
			std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), total);
#endif

		std::print(" ===== MemoryPool End =====\n");
	}
#pragma optimize("", on)
};
