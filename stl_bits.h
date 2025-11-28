#pragma once

#include <print>
#include <array>
#include <limits>
#include <bit>

#include "Observer.h"


class STL_Bits : public Observer
{
public:

	//	多bit(N)位
	template<std::size_t N>
	class BitSet
	{
	public:

		BitSet() : _bits() {}
		~BitSet() = default;
		BitSet(const BitSet&) = delete;
		BitSet& operator=(const BitSet&) = delete;

		//	设置第 n 位为 1
		inline void set(std::size_t n) noexcept
		{
			assert(n < N);
			_bits[index(n)] |= (1 << offset(n));
		}

		//	设置第 n 位为 0
		inline void reset(std::size_t n) noexcept
		{
			assert(n < N);
			_bits[index(n)] &= ~(1 << offset(n));
		}

		//	检查第 n 位是否是 1
		inline bool test(std::size_t n) const noexcept
		{
			assert(n < N);
			return _bits[index(n)] & (1 << offset(n));
		}

		//	右起连续 0 的数量, 也是第一个 1 的位置
		inline std::size_t find_first_one() const noexcept
		{
			for (int i = 0; i < kBlockNum; ++i)
			{
				auto n = std::countr_zero(_bits[i]);
				if (n) {
					return i << 6 + n;
				}
			}

			return 0;
		}

		//	右起连续 1 的数量，也是第一个 0 的位置
		inline std::size_t find_first_zero() const noexcept
		{
			for (int i = 0; i < kBlockNum; ++i)
			{
				auto n = std::countr_one(_bits[i]);
				if (n) {
					return i << 6 + n;
				}
			}

			return 0;
		}

		//	左起连续 0 的数量，也是第一个 1 的位置
		inline std::size_t find_last_one() const noexcept
		{
			for (int i = kBlockNum - 1; i >= 0; --i)
			{
				auto n = std::countl_zero(_bits[i]);
				if (n) {
					return i << 6 + n;
				}
			}

			return 0;
		}

		//	左起连续 1 的数量，也是第一个 0 的位置
		inline std::size_t find_last_zero() const noexcept
		{
			for (int i = kBlockNum - 1; i >= 0; --i)
			{
				auto n = std::countl_one(_bits[i]);
				if (n) {
					return i << 6 + n;
				}
			}

			return 0;
		}

		bool operator[](std::size_t n) const noexcept
		{
			return test(n);
		}

	private:

		using BlockType = std::uint64_t;
		static constexpr std::size_t kBlockNum = (N + 63) / 64;

		inline static constexpr std::size_t index(const std::size_t bit) noexcept
		{ 
			return bit >> 6;
		}

		inline static constexpr std::size_t offset(const std::size_t bit) noexcept
		{ 
			return bit & 63;
		}

		/*
		   kBlockNum	...		1		 0
		 |00000000...|......|......|...00000000|
		*/
		std::array<BlockType, kBlockNum> _bits;
	};

	void Test() override
	{
		std::println("===== STL_Bits Bgein =====");

		//	查找第一个1位置
		auto v = 0b00000000001001100011011000110111u;
		std::println("std::countr_zero : {}", std::countr_zero(v));
		std::println("std::countl_one : {}", std::countl_zero(v));
		
		v = 1 << 0;
		v = 0u;
		std::println("std::countr_zero ~0u : {}", std::countr_zero(v));

		std::println("===== STL_Bits End =====");
	}

};