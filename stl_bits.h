#pragma once

#include <print>
#include <array>
#include <limits>
#include <bit>
#include <bitset>
#include <assert.h>
#include <format>

#include "Observer.h"


class STL_Bits : public Observer
{
public:

	//	多bit(N 取64的倍数 )位
	template<std::size_t N>
	class BitSet
	{
	public:

		BitSet() : _bits() {
			static_assert((N % kBitsPerBlock) == 0, "N must be a multiple of 64.");
		}

		~BitSet() = default;
		BitSet(const BitSet&) = delete;
		BitSet& operator=(const BitSet&) = delete;

		//	设置第 n 位为 1
		inline void set(std::size_t n) noexcept
		{
			assert(n < N);
			_bits[index(n)] |= (1ull << offset(n));
		}

		//	设置第 n 位为 0
		inline void reset(std::size_t n) noexcept
		{
			assert(n < N);
			_bits[index(n)] &= ~(1ull << offset(n));
		}

		//	检查第 n 位是否是 1
		inline bool test(std::size_t n) const noexcept
		{
			assert(n < N);
			return _bits[index(n)] & (1ull << offset(n));
		}

		//	右起连续 0 的数量, 也是第一个 1 的位置
		inline int find_first_one() const noexcept
		{
			for (int i = 0; i < kBlockNum; ++i)
			{
				auto n = std::countr_zero(_bits[i]);
				if (n != kBitsPerBlock) {
					return (i << kBitsPerBlockLog2) + n;
				}
			}

			return -1;
		}

		//	右起连续 1 的数量，也是第一个 0 的位置
		inline int find_first_zero() const noexcept
		{
			for (int i = 0; i < kBlockNum; ++i)
			{
				auto n = std::countr_one(_bits[i]);
				if (n != kBitsPerBlock) {
					return (i << kBitsPerBlockLog2) + n;
				}
			}
			
			return -1;
		}

		//	左起连续 0 的数量，也是第一个 1 的位置
		inline int find_last_one() const noexcept
		{
			for (int i = kBlockNum - 1; i >= 0; --i)
			{
				auto n = std::countl_zero(_bits[i]);
				if (n != kBitsPerBlock) {
					return ((kBlockNum - 1 - i) << kBitsPerBlockLog2) + n;
				}
			}

			return -1;
		}

		//	左起连续 1 的数量，也是第一个 0 的位置
		inline int find_last_zero() const noexcept
		{
			for (int i = kBlockNum - 1; i >= 0; --i)
			{
				auto n = std::countl_one(_bits[i]);
				if (n != kBitsPerBlock) {
					return ((kBlockNum - 1 - i) << kBitsPerBlockLog2) + n;
				}
			}

			return -1;
		}

		inline void reset_all() noexcept
		{
			std::memset(_bits.data(), 0, sizeof(BlockType) * kBlockNum);
		}

		inline bool empty() const noexcept
		{
			for (const auto& v : _bits)
			{
				if (v != 0ull) {
					return false;
				}
			}

			return true;
		}

		inline bool full() const noexcept
		{
			for (const auto& v : _bits)
			{
				if (v != ~(0ull)) {
					return false;
				}
			}

			return true;
		}

		inline constexpr std::size_t size() const noexcept
		{ 
			return N;
		}

		inline bool operator[](std::size_t n) const noexcept
		{
			return test(n);
		}

	private:

		using BlockType = std::uint64_t;
		static constexpr std::size_t kBitsPerBlock = std::numeric_limits<BlockType>::digits;
		static constexpr std::size_t kBitsPerBlockMask = kBitsPerBlock - 1;
		static constexpr std::size_t kBitsPerBlockLog2 = std::countr_zero(kBitsPerBlock);
		static constexpr std::size_t kBlockNum = (N + kBitsPerBlockMask) / kBitsPerBlock;

		inline static constexpr std::size_t index(const std::size_t bit) noexcept
		{ 
			return bit >> kBitsPerBlockLog2;
		}

		inline static constexpr std::size_t offset(const std::size_t bit) noexcept
		{ 
			return bit & kBitsPerBlockMask;
		}

		/*
		   kBlockNum	...		 1		  0
		 |00000000 ...|......|......|... 00000000|
		*/
		std::array<BlockType, kBlockNum> _bits;
	};

	//	folly concurrent_bitset
	/**
	 * An atomic bitset of fixed size (specified at compile time).
	 *
	 * Formerly known as AtomicBitSet. It was renamed while fixing a bug
	 * to avoid any silent breakages during run time.
	 */
	template <std::size_t N>
	class ConcurrentBitSet 
	{
	public:
		
		ConcurrentBitSet() : _bits() {
			static_assert((N % kBitsPerBlock) == 0, "N must be a multiple of 64.");
		}

		ConcurrentBitSet(const ConcurrentBitSet&) = delete;
		ConcurrentBitSet& operator=(const ConcurrentBitSet&) = delete;

		/**
		 * Set bit n to true, using the given memory order. Returns the
		 * previous value of the bit.
		 *
		 * Note that the operation is a read-modify-write operation due to the use
		 * of fetch_or.
		 */
		inline bool set(std::size_t n, std::memory_order order = std::memory_order_seq_cst) noexcept
		{
			assert(n < N);
			BlockType mask = kOne << offset(n);
			return _bits[index(n)].fetch_or(mask, order) & mask;
		}

		/**
		 * Set bit n to false, using the given memory order. Returns the
		 * previous value of the bit.
		 *
		 * Note that the operation is a read-modify-write operation due to the use
		 * of fetch_and.
		 */
		inline bool reset(std::size_t n, std::memory_order order = std::memory_order_seq_cst) noexcept
		{
			assert(n < N);
			BlockType mask = kOne << offset(n);
			return _bits[index(n)].fetch_and(~mask, order) & mask;
		}

		/**
		 * Read bit n.
		 */
		inline bool test(std::size_t n, std::memory_order order = std::memory_order_acquire) const noexcept
		{
			assert(n < N);
			return _bits[index(n)].load(order) & (kOne << offset(n));
		}

		/**
		 * Same as test() with the default memory order.
		 */
		inline bool operator[](std::size_t n) const noexcept
		{
			return test(n);
		}

		/**
		 * Return the size of the bitset.
		 */
		inline constexpr std::size_t size() const noexcept
		{ 
			return N;
		}

	private:

		// Pick the largest lock-free type available
#if	  (ATOMIC_LLONG_LOCK_FREE == 2)
		typedef unsigned long long BlockType;
#elif (ATOMIC_LONG_LOCK_FREE == 2)
		typedef unsigned long BlockType;
#else
		// Even if not lock free, what can we do?
		typedef unsigned int BlockType;
#endif
		typedef std::atomic<BlockType> AtomicBlockType;

		static constexpr BlockType kOne = 1;
		static constexpr std::size_t kBitsPerBlock = std::numeric_limits<BlockType>::digits;
		static constexpr std::size_t kBitsPerBlockMask = kBitsPerBlock - 1;
		static constexpr std::size_t kBitsPerBlockLog2 = std::countr_zero(kBitsPerBlock);
		static constexpr std::size_t kNumBlocks = (N + kBitsPerBlockMask) / kBitsPerBlock;

		static constexpr std::size_t index(std::size_t bit) noexcept
		{ 
			return bit >> kBitsPerBlockLog2;
		}

		static constexpr std::size_t offset(std::size_t bit) noexcept
		{
			return bit & kBitsPerBlockMask;
		}

		// avoid casts
		std::array<AtomicBlockType, kNumBlocks> _bits;
	};

	template<std::size_t N>
	class A
	{
	public:

		inline void set(std::size_t n)
		{
			_bits |= (1ull << n);
		}

	private:

		std::uint64_t _bits;
	};

	template<std::size_t N>
	class B
	{
	public:

		inline void set(std::size_t n);

	private:

		std::uint64_t _bits;
	};

	template<std::size_t N>
	inline void B<N>::set(std::size_t n)
	{
		_bits |= (1ull << n);
	}


	void Test() override
	{
		std::println("===== STL_Bits Bgein =====");

		//	查找第一个1位置
		auto v = 0b00000000001001100011011000110111u;
		std::println("std::countr_zero : {}", std::countr_zero(v));
		std::println("std::countl_one : {}", std::countl_zero(v));
		
		v = 1 << 6;
		v = 0u;
		std::println("std::countr_zero ~0u : {}", std::countr_zero(v));

		std::bitset<200> bits;  // 自动处理多块存储
		bits.set(65);           // 安全的边界检查

		BitSet<256> bitset;
		bitset.set(3);
		auto pos = bitset.find_first_one();
		bitset.set(254);
		pos = bitset.find_last_one();
		bitset.reset(3);
		pos = bitset.find_first_one();

		for (int i = 68; i < 188; i += 2)
		{
			bitset.set(i);
		}

		auto test = bitset.test(100);
		test = bitset.test(101);
		test = bitset.test(102);

		test = bitset.empty();
		test = bitset.full();

		for (int i = 0; i < 256; ++i)
		{
			bitset.set(i);
		}

		test = bitset.empty();
		test = bitset.full();

		bitset.reset_all();
		test = bitset.empty();
		test = bitset.full();

		ConcurrentBitSet<64> cbitset;
		cbitset.set(5);
		auto ctest = cbitset.test(5);

		std::println("Bitset size : {}", sizeof bitset);
		std::println("concurrentBitset size : {}", sizeof cbitset);

		std::println("===== STL_Bits End =====");
	}

};