#pragma once

#include <print>
#include <memory>
#include <memory_resource>
#include <cassert>

#include "SpinLock.h"
#include "Observer.h"

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
		auto constexpr buf_address() const noexcept { return std::uintptr_t(_buf); }
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
	template<class T, std::size_t BLOCK_SIZE = 128, bool is_trivial = std::is_trivial_v<T>>
	class SimpleMemoryPool
	{
	public:

		SimpleMemoryPool() : _head(nullptr)
		{
			expand();
		};

		~SimpleMemoryPool()
		{
			for (const auto& block : _blocks) {
				::operator delete(block, std::align_val_t{ alignof(MemoryNode) });
			}
		}

		SimpleMemoryPool(SimpleMemoryPool&& other) noexcept
			: _head(other._head.load(std::memory_order_acquire))
			, _blocks(std::move(other._blocks))
		{
			other._head.store(nullptr, std::memory_order_release);
			other._blocks.clear();
		}

		SimpleMemoryPool(const SimpleMemoryPool&) = delete;
		SimpleMemoryPool& operator=(const SimpleMemoryPool&) = delete;
		SimpleMemoryPool& operator=(SimpleMemoryPool&& other) = delete;

		template<typename... Args>
		[[nodiscard]] T* Alloc(Args&&... args)
		{
			MemoryNode* node;
			do {
				node = _head.load(std::memory_order_acquire);
				if (!node) [[unlikely]] {
					expand();
					node = _head.load(std::memory_order_acquire);
				}
			} while (!_head.compare_exchange_weak(
				node,
				node->next.load(std::memory_order_relaxed),
				std::memory_order_acq_rel,
				std::memory_order_acquire));

			new (node->data) T(std::forward<Args>(args)...);
			return reinterpret_cast<T*>(node->data);
		}

		[[nodiscard]] T* Alloc(T&& o)
		{
			MemoryNode* node;
			do {
				node = _head.load(std::memory_order_acquire);
				if (!node) [[unlikely]] {
					expand();
					node = _head.load(std::memory_order_acquire);
				}
			} while (!_head.compare_exchange_weak(
				node,
				node->next.load(std::memory_order_relaxed),
				std::memory_order_acq_rel,
				std::memory_order_acquire));

			new (node->data) T(std::forward<T>(o));
			return reinterpret_cast<T*>(node->data);
		}

		[[nodiscard]] T* Alloc()
		{
			MemoryNode* node;
			do {
				node = _head.load(std::memory_order_acquire);
				if (!node) [[unlikely]] {
					expand();
					node = _head.load(std::memory_order_acquire);
				}
			} while (!_head.compare_exchange_weak(
				node,
				node->next.load(std::memory_order_relaxed),
				std::memory_order_acq_rel,
				std::memory_order_acquire));

			if constexpr (!is_trivial) {
				new (node->data) T();
			}
			return reinterpret_cast<T*>(node->data);
		}

		void Recycle(T* o)
		{
			if (!o) [[unlikely]] {
				return;
			}

			if constexpr (!is_trivial) {
				o->~T();
			}

			MemoryNode* node = reinterpret_cast<MemoryNode*>(
				reinterpret_cast<std::byte*>(o) - offsetof(MemoryNode, data));
			MemoryNode* expected = _head.load(std::memory_order_acquire);
			do {
				node->next.store(expected, std::memory_order_relaxed);
			} while (!_head.compare_exchange_weak(
				expected,
				node,
				std::memory_order_acq_rel,
				std::memory_order_acquire));
		}

		std::size_t Capacity() const noexcept {
			return _blocks.size() * BLOCK_SIZE;
		}

	private:

		struct MemoryNode {
			alignas(T) std::byte data[sizeof(T)];
			std::atomic<MemoryNode*> next{ nullptr };
		};
		static_assert(sizeof(T) > 0, "T must be a complete type");
		static_assert(offsetof(SimpleMemoryPool::MemoryNode, data) == 0,
			"MemoryNode.data must be the first member");

		void expand()
		{
			MemoryNode* block = static_cast<MemoryNode*>(::operator new(
				sizeof(MemoryNode) * BLOCK_SIZE, std::align_val_t{ alignof(MemoryNode) }));
			if (!block) {
				throw std::bad_alloc();
				return;
			}

			MemoryNode* node = block;
			for (std::size_t i = 1; i < BLOCK_SIZE; i++) {
				node->next.store(block + i, std::memory_order_relaxed);
				node = block + i;
			}
			_blocks.push_back(block);

			MemoryNode* expected = _head.load(std::memory_order_acquire);
			do {
				node->next.store(expected, std::memory_order_relaxed);
			} while (!_head.compare_exchange_weak(
				expected,
				block,
				std::memory_order_acq_rel,
				std::memory_order_acquire));
		}

		std::atomic<MemoryNode*> _head;
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

		//	普通vector分配内存，成员在堆空间中分配
		std::print("alignof(std::max_align_t) : {}.\n", alignof(std::max_align_t));
		std::print("sizeof std::vector   : {}.\n", sizeof std::vector<MyStruct>);
		std::vector<MyStruct> normal_vec;
		normal_vec.reserve(32);
		for (int i = 0; i < 4; i++)
		{
			normal_vec.emplace_back(i, i);
		}
		std::print("normal_vec address : {}.\n", std::uintptr_t(&normal_vec));
		for (const auto& item : normal_vec)
		{
			//	成员地址在堆空间中分配
			std::print("normal item address: {}.\n", std::uintptr_t(&item));
		}

		//	使用栈空间分配内存
		using allocator_type = StackPool<MyStruct, 
			(64 * sizeof(MyStruct) + (alignof(std::max_align_t) - 1)) & ~(alignof(std::max_align_t) - 1)>;
		using arena_type = typename allocator_type::arena_type;
		using stack_storage = std::vector<MyStruct, allocator_type>;
		arena_type arena_pool;
		stack_storage stack_vec(arena_pool);
		stack_vec.reserve(32);
		for (int i = 0; i < 4; i++)
		{
			stack_vec.emplace_back(i, i * 10);
		}

		std::print("sizeof stack_storage : {}.\n", sizeof stack_storage);
		std::print("stack_vec address  : {}.\n", std::uintptr_t(&stack_vec));
		std::print("arena address      : {}.\n", std::uintptr_t(&arena_pool));
		std::print("arena buf address  : {}.\n", arena_pool.buf_address());	//	与arena地址相同
		
		for (const auto& item : stack_vec)
		{
			//	成员地址在栈空间中分配
			std::print("stack item address : {}.\n", std::uintptr_t(&item));
		}

		//	使用 std::pmr 分配内存，类似于上面的栈空间分配
		//	std::pmr 更加通用，运行时绑定可以动态调整内存资源，效率略低于stackpool
		char buffer[64 * sizeof(MyStruct)];	//	预分配栈空间
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
			//	成员地址在buffer空间中分配
			std::print("pmr item address   : {}.\n", std::uintptr_t(&item));
		}
		
		//	使用链表内存池分配内存
		SimpleMemoryPool<MyStruct> pool;
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
#if 1
						MyStruct* p = pool.Alloc(i, j);
						if (nullptr == p)
						{
							std::print("malloc failed! \n");
							return;
						}
						total += (p->value & 0xffff);
						pool.Recycle(p);
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
		std::print("MemoryPool 内存池测试 + {}, 共耗时 : {}ms, Total : {}.\n", "Automic",
			std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), total);
		//	使用 Spinlock 耗时 1000 - 1200 ms 左右
		//	使用 Spinlock2 耗时 600 - 800 ms 左右
		//	使用 std::mutex 耗时 300 - 350 ms 左右
#else
		std::print("MemoryPool ::new 共耗时 : {}ms, Total : {}.\n",
			std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), total);
		//	耗时 200 ms 左右
#endif
		//	SpinLock 比 std::mutex 性能慢了三倍。。。??
		std::print(" ===== MemoryPool End =====\n");
	}
#pragma optimize("", on)
};
