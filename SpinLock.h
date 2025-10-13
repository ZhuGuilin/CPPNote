#pragma once

#include <atomic>
#include <thread>

#include "define.h"
#include "Observer.h"

inline void asm_volatile_pause() {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
	::_mm_pause();
#elif defined(__i386__) || (defined(__x86_64__) || defined(_M_X64)) || \
    (defined(__mips_isa_rev) && __mips_isa_rev > 1)
	asm volatile("pause");
#elif defined(__aarch64__)
#if __ARM_ARCH >= 9
	asm volatile("sb");
#else
	asm volatile("isb");
#endif
#elif (defined(__arm__) && !(__ARM_ARCH < 7))
	asm volatile("yield");
#elif defined(__powerpc64__)
	asm volatile("lwsync");
#endif
}

#if  (defined(__x86_64__) || defined(_M_X64))
constexpr uint32_t MAX_PAUSE_COUNT{ 200 };
constexpr uint32_t MAX_YIELD_COUNT{ 600 };
#elif defined(__arm__)
constexpr uint32_t MAX_PAUSE_COUNT{ 400 };
constexpr uint32_t MAX_YIELD_COUNT{ 800 };
#else
constexpr uint32_t MAX_PAUSE_COUNT{ 500 };
constexpr uint32_t MAX_YIELD_COUNT{ 1000 };
#endif

constexpr uint32_t MAX_COUNT{ MAX_YIELD_COUNT * 2 };
constexpr std::chrono::microseconds SLEEP_MICRO{ 200 };

class MS_Lock : public Observer
{
public:

	//	简单的自旋锁实现 (不支持递归锁)	
	//	除非临界区非常短使用自旋锁能带来确定的收益，否则不建议使用自旋锁
	//	std::mutex在高争用时会使用自旋锁+阻塞的方式来提高性能
	//	考虑使用无锁数据结构替代锁
	class Spinlock
	{
	public:

		void lock() noexcept {
			uint32_t count = 0;
			while (flag.test_and_set(std::memory_order_acquire)) {
				if (count < MAX_COUNT) {
					++count;
				}

				if (count <= MAX_PAUSE_COUNT) {
					asm_volatile_pause();
				}
				else if (count <= MAX_YIELD_COUNT) {
#if defined(__arm__) && !(__ARM_ARCH < 7)
					asm volatile("yield" ::: "memory");
#else
					std::this_thread::yield();
#endif
				}
				else {
					auto backoff = std::chrono::microseconds(1 << (count - MAX_YIELD_COUNT) / 10);
					std::this_thread::sleep_for(std::min(backoff, SLEEP_MICRO));
				}
			}
		}
		void unlock() noexcept {
			flag.clear(std::memory_order_release);
		}
		bool trylock() noexcept {
			return !flag.test_and_set(std::memory_order_acquire);
		}

	private:

		alignas(64) std::atomic_flag flag = ATOMIC_FLAG_INIT;
	};

	void Test() override
	{
		std::cout << " ===== SpinLock Bgein =====" << std::endl;
		Spinlock slock;
		std::lock_guard<Spinlock> lock(slock);
		std::cout << " ===== SpinLock End =====" << std::endl;
	}

};
