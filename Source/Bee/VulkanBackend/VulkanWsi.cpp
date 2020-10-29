/*
 *  VulkanWsi.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/VulkanBackend/VulkanDevice.hpp"
#include "Bee/Platform/Platform.hpp"

namespace bee {


VkSurfaceKHR vk_create_wsi_surface(VkInstance instance, void* os_window)
{
#if BEE_OS_WINDOWS == 1
    VkWin32SurfaceCreateInfoKHR surface_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    surface_info.hwnd = static_cast<HWND>(os_window);
    surface_info.hinstance = GetModuleHandle(nullptr);

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    BEE_VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface));
    return surface;
#endif // BEE_OS_WINDOWS == 1
}


} // namespace bee