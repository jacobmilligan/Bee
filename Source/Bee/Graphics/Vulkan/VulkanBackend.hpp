/*
 *  Vulkan.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

#if BEE_OS_WINDOWS == 1
    #define VK_USE_PLATFORM_WIN32_KHR
#endif // BEE_OS_WINDOWS == 1

#include <volk.h>

BEE_PUSH_WARNING
    BEE_DISABLE_WARNING_MSVC(4127) // conditional expression is constant
    BEE_DISABLE_WARNING_MSVC(4189) // local variable is initialized but not referenced
    BEE_DISABLE_WARNING_MSVC(4701) // potentially uninitialized local variable used
    BEE_DISABLE_WARNING_MSVC(4324) // 'VmaPoolAllocator<VmaAllocation_T>::Item': structure was padded due to alignment specifier

    #include <vk_mem_alloc.h>

BEE_POP_WARNING

namespace bee {

struct VulkanDevice
{
    VkDevice handle;
};


} // namespace bee