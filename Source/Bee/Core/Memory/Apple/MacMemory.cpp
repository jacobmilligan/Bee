/*
 *  MacMemory.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

// TODO(Jacob): Leaving this here for when I do windows.
// stack size always gets rounded up to the nearest multiple of the systems granularity (64kb)
// see: https://docs.microsoft.com/en-us/windows/desktop/ProcThread/thread-stack-size
// #define BEE_MIN_STACK_SIZE 65536 (64k)
// #define BEE_CANONICAL_STACK_SIZE 131072 (128k)

#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Meta.hpp"

#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

namespace bee {


size_t get_page_size() noexcept
{
    // note: getpagesize() is deprecated in macOS POSIX
    return sysconf(_SC_PAGESIZE);
}

size_t get_min_stack_size() noexcept
{
    return MINSIGSTKSZ;
}

size_t get_max_stack_size() noexcept
{
    rlimit resource_limit{};

    if (BEE_FAIL_F(getrlimit(RLIMIT_STACK, &resource_limit) == 0, "Failed to get resource limits: errno: %s", strerror(errno))) {
        return 0;
    }

    return resource_limit.rlim_max;
}

size_t get_canonical_stack_size() noexcept
{
    return SIGSTKSZ;
}

bool guard_memory(void* memory, const size_t num_bytes, const MemoryProtectionMode protection) noexcept
{
    auto prot = decode_flag(protection, MemoryProtectionMode::none, PROT_NONE);
    prot |= decode_flag(protection, MemoryProtectionMode::read, PROT_READ);
    prot |= decode_flag(protection, MemoryProtectionMode::write, PROT_WRITE);
    prot |= decode_flag(protection, MemoryProtectionMode::exec, PROT_EXEC);

    const auto mprotect_success = mprotect(memory, num_bytes, prot);
    BEE_ASSERT_F(mprotect_success == 0,
        "Failed to make memory guard for address: %p, MemoryProtectionMode: %d, error: %s",
        memory, underlying_t(protection), strerror(errno));

    return mprotect_success == 0;
}


} // namespace bee