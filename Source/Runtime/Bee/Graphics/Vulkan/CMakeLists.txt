find_package(Vulkan)
if (Vulkan_FOUND)
    bee_log("Support for Vulkan backend detected")
    bee_log("Using Vulkan SDK library from ${Vulkan_LIBRARIES}")
    bee_log("Using Vulkan SDK headers from ${Vulkan_INCLUDE_DIRS}")

    # Compile all the vulkan backend sources if the vulkan1.dll is available
    bee_add_include_dirs(${Vulkan_INCLUDE_DIRS})

    bee_add_sources(
        VulkanBackend.hpp
        VulkanBackend.hpp   VulkanBackend.cpp
        VulkanCommands.cpp
        VulkanConvert.hpp   VulkanConvert.cpp

        # volk
        ${BEE_THIRD_PARTY}/volk/volk.h  ${BEE_THIRD_PARTY}/volk/volk.c

        # Vulkan Memory Allocator
        VMA.cpp
    )

    if (WIN32)
        bee_add_sources(Win32Vulkan.cpp)
        bee_add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
    endif ()
endif ()

