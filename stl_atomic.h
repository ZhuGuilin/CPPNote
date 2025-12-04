#pragma once

#include <print>
#include <atomic>
#include <cstdint>

#include "Observer.h"

#include "AtomicStruct.h"


class STL_Atomic : public Observer
{
public:

    //  copy from folly
    //  使用std::memory_order_relaxed内存顺序的原子对象
    //  std::atomic默认使用std::memoryorderseq_cst
    //  适用于可以同时加载和存储的值，例如
    //  计数器，但不保护任何相关数据
    template <typename T>
    struct relaxed_atomic;

    template <typename T>
    struct relaxed_atomic_base : protected std::atomic<T> 
    {
    private:

        using atomic = std::atomic<T>;

    public:

        using value_type = T;
        using atomic::atomic;

        T operator=(T desired) noexcept 
        {
            store(desired);
            return desired;
        }

        T operator=(T desired) volatile noexcept 
        {
            store(desired);
            return desired;
        }

        bool is_lock_free() const noexcept { return atomic::is_lock_free(); }
        bool is_lock_free() const volatile noexcept { return atomic::is_lock_free(); }

        void store(T desired) noexcept { atomic::store(desired, std::memory_order_relaxed); }
        void store(T desired) volatile noexcept { atomic::store(desired, std::memory_order_relaxed); }

        T load() const noexcept { return atomic::load(std::memory_order_relaxed); }
        T load() const volatile noexcept { return atomic::load(std::memory_order_relaxed); }

        operator T() const noexcept { return load(); }
        operator T() const volatile noexcept { return load(); }

        T exchange(T desired) noexcept { return atomic::exchange(desired, std::memory_order_relaxed); }
        T exchange(T desired) volatile noexcept { return atomic::exchange(desired, std::memory_order_relaxed); }

        bool compare_exchange_weak(T& expected, T desired) noexcept 
        {
            return atomic::compare_exchange_weak(expected, desired, std::memory_order_relaxed);
        }

        bool compare_exchange_weak(T& expected, T desired) volatile noexcept 
        {
            return atomic::compare_exchange_weak(expected, desired, std::memory_order_relaxed);
        }

        bool compare_exchange_strong(T& expected, T desired) noexcept 
        {
            return atomic::compare_exchange_strong(expected, desired, std::memory_order_relaxed);
        }

        bool compare_exchange_strong(T& expected, T desired) volatile noexcept 
        {
            return atomic::compare_exchange_strong(expected, desired, std::memory_order_relaxed);
        }
    };

    template <typename T>
    struct relaxed_atomic_integral_base : private relaxed_atomic_base<T>
    {
    private:

        using atomic = std::atomic<T>;
        using base = relaxed_atomic_base<T>;

    public:

        using typename base::value_type;

        using base::relaxed_atomic_base;
        using base::operator=;
        using base::operator T;
        using base::compare_exchange_strong;
        using base::compare_exchange_weak;
        using base::exchange;
        using base::is_lock_free;
        using base::load;
        using base::store;

        T fetch_add(T arg) noexcept {
            return atomic::fetch_add(arg, std::memory_order_relaxed);
        }
        T fetch_add(T arg) volatile noexcept {
            return atomic::fetch_add(arg, std::memory_order_relaxed);
        }

        T fetch_sub(T arg) noexcept {
            return atomic::fetch_sub(arg, std::memory_order_relaxed);
        }
        T fetch_sub(T arg) volatile noexcept {
            return atomic::fetch_sub(arg, std::memory_order_relaxed);
        }

        T fetch_and(T arg) noexcept {
            return atomic::fetch_and(arg, std::memory_order_relaxed);
        }
        T fetch_and(T arg) volatile noexcept {
            return atomic::fetch_and(arg, std::memory_order_relaxed);
        }

        T fetch_or(T arg) noexcept {
            return atomic::fetch_or(arg, std::memory_order_relaxed);
        }
        T fetch_or(T arg) volatile noexcept {
            return atomic::fetch_or(arg, std::memory_order_relaxed);
        }

        T fetch_xor(T arg) noexcept {
            return atomic::fetch_xor(arg, std::memory_order_relaxed);
        }
        T fetch_xor(T arg) volatile noexcept {
            return atomic::fetch_xor(arg, std::memory_order_relaxed);
        }

        T operator++() noexcept { return fetch_add(1) + 1; }
        T operator++() volatile noexcept { return fetch_add(1) + 1; }

        T operator++(int) noexcept { return fetch_add(1); }
        T operator++(int) volatile noexcept { return fetch_add(1); }

        T operator--() noexcept { return fetch_sub(1) - 1; }
        T operator--() volatile noexcept { return fetch_sub(1) - 1; }

        T operator--(int) noexcept { return fetch_sub(1); }
        T operator--(int) volatile noexcept { return fetch_sub(1); }

        T operator+=(T arg) noexcept { return fetch_add(arg) + arg; }
        T operator+=(T arg) volatile noexcept { return fetch_add(arg) + arg; }

        T operator-=(T arg) noexcept { return fetch_sub(arg) - arg; }
        T operator-=(T arg) volatile noexcept { return fetch_sub(arg) - arg; }

        T operator&=(T arg) noexcept { return fetch_and(arg) & arg; }
        T operator&=(T arg) volatile noexcept { return fetch_and(arg) & arg; }

        T operator|=(T arg) noexcept { return fetch_or(arg) | arg; }
        T operator|=(T arg) volatile noexcept { return fetch_or(arg) | arg; }

        T operator^=(T arg) noexcept { return fetch_xor(arg) ^ arg; }
        T operator^=(T arg) volatile noexcept { return fetch_xor(arg) ^ arg; }
    };

    template <>
    struct relaxed_atomic<bool> : relaxed_atomic_base<bool> {
    private:
        using base = relaxed_atomic_base<bool>;

    public:
        using typename base::value_type;

        using base::relaxed_atomic_base;
        using base::operator=;
        using base::operator bool;
    };

    using relaxed_atomic_bool = relaxed_atomic<bool>;

    template <typename T>
    struct relaxed_atomic : relaxed_atomic_base<T> {
    private:
        using base = relaxed_atomic_base<T>;

    public:
        using typename base::value_type;

        using base::relaxed_atomic_base;
        using base::operator=;
        using base::operator T;
    };

    template <typename T>
    struct relaxed_atomic<T*> : relaxed_atomic_base<T*> {
    private:
        using atomic = std::atomic<T*>;
        using base = relaxed_atomic_base<T*>;

    public:
        using typename base::value_type;

        using relaxed_atomic_base<T*>::relaxed_atomic_base;
        using base::operator=;
        using base::operator T*;

        T* fetch_add(std::ptrdiff_t arg) noexcept {
            return atomic::fetch_add(arg, std::memory_order_relaxed);
        }
        T* fetch_add(std::ptrdiff_t arg) volatile noexcept {
            return atomic::fetch_add(arg, std::memory_order_relaxed);
        }

        T* fetch_sub(std::ptrdiff_t arg) noexcept {
            return atomic::fetch_sub(arg, std::memory_order_relaxed);
        }
        T* fetch_sub(std::ptrdiff_t arg) volatile noexcept {
            return atomic::fetch_sub(arg, std::memory_order_relaxed);
        }

        T* operator++() noexcept { return fetch_add(1) + 1; }
        T* operator++() volatile noexcept { return fetch_add(1) + 1; }

        T* operator++(int) noexcept { return fetch_add(1); }
        T* operator++(int) volatile noexcept { return fetch_add(1); }

        T* operator--() noexcept { return fetch_sub(1) - 1; }
        T* operator--() volatile noexcept { return fetch_sub(1) - 1; }

        T* operator--(int) noexcept { return fetch_sub(1); }
        T* operator--(int) volatile noexcept { return fetch_sub(1); }

        T* operator+=(std::ptrdiff_t arg) noexcept { return fetch_add(arg) + arg; }
        T* operator+=(std::ptrdiff_t arg) volatile noexcept {
            return fetch_add(arg) + arg;
        }

        T* operator-=(std::ptrdiff_t arg) noexcept { return fetch_sub(arg) - arg; }
        T* operator-=(std::ptrdiff_t arg) volatile noexcept {
            return fetch_sub(arg) - arg;
        }
    };

    template <typename T>
    relaxed_atomic(T) -> relaxed_atomic<T>;

#define RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(type)            \
    template <>                                                        \
    struct relaxed_atomic<type> : relaxed_atomic_integral_base<type> { \
    private:                                                           \
        using base = relaxed_atomic_integral_base<type>;               \
                                                                       \
    public:                                                            \
        using base::relaxed_atomic_integral_base;                      \
        using base::operator=;                                         \
        using base::operator type;                                     \
    };

    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(char)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(signed char)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(unsigned char)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(signed short)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(unsigned short)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(signed int)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(unsigned int)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(signed long)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(unsigned long)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(signed long long)
    RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION(unsigned long long)

#undef RELAXED_ATOMIC_DEFINE_INTEGRAL_SPECIALIZATION

    using relaxed_atomic_char = relaxed_atomic<char>;
    using relaxed_atomic_schar = relaxed_atomic<signed char>;
    using relaxed_atomic_uchar = relaxed_atomic<unsigned char>;
    using relaxed_atomic_short = relaxed_atomic<short>;
    using relaxed_atomic_ushort = relaxed_atomic<unsigned short>;
    using relaxed_atomic_int = relaxed_atomic<int>;
    using relaxed_atomic_uint = relaxed_atomic<unsigned int>;
    using relaxed_atomic_long = relaxed_atomic<long>;
    using relaxed_atomic_ulong = relaxed_atomic<unsigned long>;
    using relaxed_atomic_llong = relaxed_atomic<long long>;
    using relaxed_atomic_ullong = relaxed_atomic<unsigned long long>;
    using relaxed_atomic_char16_t = relaxed_atomic<char16_t>;
    using relaxed_atomic_char32_t = relaxed_atomic<char32_t>;
    using relaxed_atomic_wchar_t = relaxed_atomic<wchar_t>;
    using relaxed_atomic_int8_t = relaxed_atomic<std::int8_t>;
    using relaxed_atomic_uint8_t = relaxed_atomic<std::uint8_t>;
    using relaxed_atomic_int16_t = relaxed_atomic<std::int16_t>;
    using relaxed_atomic_uint16_t = relaxed_atomic<std::uint16_t>;
    using relaxed_atomic_int32_t = relaxed_atomic<std::int32_t>;
    using relaxed_atomic_uint32_t = relaxed_atomic<std::uint32_t>;
    using relaxed_atomic_int64_t = relaxed_atomic<std::int64_t>;
    using relaxed_atomic_uint64_t = relaxed_atomic<std::uint64_t>;
    using relaxed_atomic_least8_t = relaxed_atomic<std::int_least8_t>;
    using relaxed_atomic_uleast8_t = relaxed_atomic<std::uint_least8_t>;
    using relaxed_atomic_least16_t = relaxed_atomic<std::int_least16_t>;
    using relaxed_atomic_uleast16_t = relaxed_atomic<std::uint_least16_t>;
    using relaxed_atomic_least32_t = relaxed_atomic<std::int_least32_t>;
    using relaxed_atomic_uleast32_t = relaxed_atomic<std::uint_least32_t>;
    using relaxed_atomic_least64_t = relaxed_atomic<std::int_least64_t>;
    using relaxed_atomic_uleast64_t = relaxed_atomic<std::uint_least64_t>;
    using relaxed_atomic_fast8_t = relaxed_atomic<std::int_fast8_t>;
    using relaxed_atomic_ufast8_t = relaxed_atomic<std::uint_fast8_t>;
    using relaxed_atomic_fast16_t = relaxed_atomic<std::int_fast16_t>;
    using relaxed_atomic_ufast16_t = relaxed_atomic<std::uint_fast16_t>;
    using relaxed_atomic_fast32_t = relaxed_atomic<std::int_fast32_t>;
    using relaxed_atomic_ufast32_t = relaxed_atomic<std::uint_fast32_t>;
    using relaxed_atomic_fast64_t = relaxed_atomic<std::int_fast64_t>;
    using relaxed_atomic_ufast64_t = relaxed_atomic<std::uint_fast64_t>;
    using relaxed_atomic_intptr_t = relaxed_atomic<std::intptr_t>;
    using relaxed_atomic_uintptr_t = relaxed_atomic<std::uintptr_t>;
    using relaxed_atomic_size_t = relaxed_atomic<std::size_t>;
    using relaxed_atomic_ptrdiff_t = relaxed_atomic<std::ptrdiff_t>;
    using relaxed_atomic_intmax_t = relaxed_atomic<std::intmax_t>;
    using relaxed_atomic_uintmax_t = relaxed_atomic<std::uintmax_t>;

	void Test() override
	{
		std::println("===== STL_Atomic Bgein =====");

        relaxed_atomic_ufast64_t count;
        ++count;

		std::println("===== STL_Atomic End =====");
	}

};