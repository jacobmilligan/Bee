/*
 *  VMA.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Error.hpp" // Must be included before Vulkan.hpp

#define VMA_ASSERT(expr) BEE_ASSERT(expr)
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"