/*
 *  Win32Surface.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"


namespace bee {


VkSurfaceKHR gpu_create_wsi_surface(VkInstance instance, const WindowHandle& window)
{
    VkWin32SurfaceCreateInfoKHR surface_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    surface_info.hwnd = static_cast<HWND>(get_os_window(window));
    surface_info.hinstance = GetModuleHandle(nullptr);

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    BEE_VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface));
    return surface;
}


} // namespace bee