find_package(Vulkan)
if (Vulkan_FOUND)
    bee_new_source_root()

    bee_log("Support for Vulkan backend detected")
    bee_log("Using Vulkan SDK library from ${Vulkan_LIBRARIES}")
    bee_log("Using Vulkan SDK headers from ${Vulkan_INCLUDE_DIRS}")

    # Compile all the vulkan backend sources if the vulkan1.dll is available
    bee_add_include_dirs(${Vulkan_INCLUDE_DIRS})

    bee_add_sources(
            VulkanBackend.hpp
            VulkanCommand.cpp
            VulkanConvert.hpp       VulkanConvert.cpp
            VulkanDevice.hpp        VulkanDevice.cpp
            VulkanStaging.cpp
            VulkanObjectCache.hpp   VulkanObjectCache.cpp
            VulkanWsi.cpp

            # volk
            ${BEE_THIRD_PARTY}/volk/volk.h  ${BEE_THIRD_PARTY}/volk/volk.c
    )

    if (WIN32)
        # required by vulkan SDK
        bee_add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
    endif ()

    bee_plugin(Bee.VulkanBackend LINK_LIBRARIES Bee.Core)
endif ()