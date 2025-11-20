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
constexpr uint32_t MAX_PAUSE_COUNT{ 400 };
#elif defined(__arm__)
constexpr uint32_t MAX_PAUSE_COUNT{ 800 };
#else
constexpr uint32_t MAX_PAUSE_COUNT{ 1200 };
#endif

class MS_Lock : public Observer
{
public:

	//	简单的自旋锁实现 (不支持递归锁)	
	//	除非临界区非常短使用自旋锁能带来确定的收益，否则不建议使用自旋锁
	//  根据本人测试后的理解，在资源高竞争环境下（>= 8线程）自旋锁可能带来意想不到的糟糕性能
	//	std::mutex可能更稳定
	//	考虑使用无锁数据结构替代锁
	//	以下是来自folly库对于自旋锁的建议：
	/*
	 * N.B. You most likely do _not_ want to use SpinLock or any other
	 * kind of spinlock.  Use std::mutex instead.
	 *
	 * In short, spinlocks in preemptive multi-tasking operating systems
	 * have serious problems and fast mutexes like std::mutex are almost
	 * certainly the better choice, because letting the OS scheduler put a
	 * thread to sleep is better for system responsiveness and throughput
	 * than wasting a timeslice repeatedly querying a lock held by a
	 * thread that's blocked, and you can't prevent userspace
	 * programs blocking.
	 *
	 * Spinlocks in an operating system kernel make much more sense than
	 * they do in userspace.
	 */

	class Spinlock
	{
	public:

		void lock() noexcept {
			uint32_t count = 0;
			while (_flag.test_and_set(std::memory_order_acquire)) {
				if (count++ <= MAX_PAUSE_COUNT) {
					asm_volatile_pause();
				}
				else {
#if defined(__arm__) && !(__ARM_ARCH < 7)
					asm volatile("yield" ::: "memory");
#else
					std::this_thread::yield();
#endif
				}
			}
		}

		void unlock() noexcept {
			_flag.clear(std::memory_order_release);
		}

		bool trylock() noexcept {
			return !_flag.test_and_set(std::memory_order_acquire);
		}

	private:

		alignas(64) std::atomic_flag _flag = ATOMIC_FLAG_INIT;
	};

	class Spinlock2
	{
	public:

		void lock() noexcept
		{
			while (_flag.test_and_set(std::memory_order_acquire))
			{
				//asm_volatile_pause();
				std::this_thread::yield();	//	在memorypool.h测试中发现直接yield性能更好??
			}
		}

		void unlock() noexcept 
		{
			_flag.clear(std::memory_order_release);
		}

		bool trylock() noexcept 
		{
			return !_flag.test_and_set(std::memory_order_acquire);
		}

	private:

		alignas(std::hardware_destructive_interference_size) std::atomic_flag _flag = ATOMIC_FLAG_INIT;
	};

	void Test() override
	{
		std::print(" ===== SpinLock Bgein =====\n");

		Spinlock slock;
		std::lock_guard<Spinlock> lock(slock);

		std::print(" ===== SpinLock End =====\n");
	}

};
