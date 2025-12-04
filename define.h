#pragma once

#ifndef _WIN32

#include <unistd.h>

#else

#include <cstdint>
#include <sys/types.h>

#endif


#ifdef _MSC_VER
#define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#else
#define ALWAYS_INLINE inline
#endif

// This is different from the normal headers because there are a few cases,
// such as close(), where we need to override the definition of an existing
// function. To avoid conflicts at link time, everything here is in a namespace
// which is then used globally.
#define _SC_PAGESIZE 1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 2
#define _SC_NPROCESSORS_CONF 2

static ALWAYS_INLINE long sc_page_size() 
{
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    ::GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}

[[noreturn]] ALWAYS_INLINE void compiler_may_unsafely_assume_unreachable() 
{
#if defined(__GNUC__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(0);
#else
    while (!0)
        ;
#endif
}

inline std::memory_order default_failure_memory_order
(
    std::memory_order successMode) {
    switch (successMode) {
    case std::memory_order_acq_rel:
        return std::memory_order_acquire;
    case std::memory_order_release:
        return std::memory_order_relaxed;
    case std::memory_order_relaxed:
    case std::memory_order_consume:
    case std::memory_order_acquire:
    case std::memory_order_seq_cst:
        return successMode;
    }

    compiler_may_unsafely_assume_unreachable();
}

#define MAP_ANONYMOUS 1
#define MAP_ANON MAP_ANONYMOUS
#define MAP_SHARED 2
#define MAP_PRIVATE 4
#define MAP_POPULATE 8
#define MAP_NORESERVE 16
#define MAP_FIXED 32
#define MAP_FAILED ((void*)-1)

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

extern "C" {
    int madvise(const void* addr, size_t len, int advise);
    int mlock(const void* addr, size_t len);
    void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t off);
    void* mmap64(
        void* addr, size_t length, int prot, int flags, int fd, int64_t off);
    int mprotect(void* addr, size_t size, int prot);
    int munlock(const void* addr, size_t length);
    int munmap(void* addr, size_t length);
}