/*
 *  Vulkan.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Debug.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Graphics/GPULimits.hpp"
#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"
#include "Bee/Graphics/Vulkan/VulkanConvert.hpp"


namespace bee {


/*
 ************************************************
 *
 * # Vulkan helper functions and debug callbacks
 *
 ************************************************
 */
#define BEE_VK_CHECK(fn) BEE_BEGIN_MACRO_BLOCK                                                  \
            const auto result = fn;                                                             \
            BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));    \
        BEE_END_MACRO_BLOCK

#define BEE_VMA_CHECK(fn) BEE_BEGIN_MACRO_BLOCK                                                                                     \
            const auto result = fn;                                                                                                 \
            BEE_ASSERT_F(result != VK_ERROR_VALIDATION_FAILED_EXT, "Vulkan Memory Allocator tried to allocate zero-sized memory");  \
            BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));                                        \
        BEE_END_MACRO_BLOCK

const char* vk_result_string(VkResult result)
{
#define BEE_VKRESULT_NAME(r, str) case r : return str;
    switch (result) {
        BEE_VKRESULT_NAME(VK_SUCCESS, "Success")
        BEE_VKRESULT_NAME(VK_NOT_READY, "Not ready")
        BEE_VKRESULT_NAME(VK_TIMEOUT, "Timeout")
        BEE_VKRESULT_NAME(VK_EVENT_SET, "Event set")
        BEE_VKRESULT_NAME(VK_EVENT_RESET, "Event rest")
        BEE_VKRESULT_NAME(VK_INCOMPLETE, "Incomplete")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_HOST_MEMORY, "Out of host memory")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_DEVICE_MEMORY, "Out of device memory")
        BEE_VKRESULT_NAME(VK_ERROR_INITIALIZATION_FAILED, "Initialization failed")
        BEE_VKRESULT_NAME(VK_ERROR_DEVICE_LOST, "GraphicsDevice lost")
        BEE_VKRESULT_NAME(VK_ERROR_MEMORY_MAP_FAILED, "Memory map failed")
        BEE_VKRESULT_NAME(VK_ERROR_LAYER_NOT_PRESENT, "Layer not present")
        BEE_VKRESULT_NAME(VK_ERROR_EXTENSION_NOT_PRESENT, "Extension not present")
        BEE_VKRESULT_NAME(VK_ERROR_FEATURE_NOT_PRESENT, "Feature not present")
        BEE_VKRESULT_NAME(VK_ERROR_INCOMPATIBLE_DRIVER, "Incompatible driver")
        BEE_VKRESULT_NAME(VK_ERROR_TOO_MANY_OBJECTS, "Too many objects")
        BEE_VKRESULT_NAME(VK_ERROR_FORMAT_NOT_SUPPORTED, "Format not supported")
        BEE_VKRESULT_NAME(VK_ERROR_FRAGMENTED_POOL, "Fragmented pool")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_POOL_MEMORY, "Out of pool memory")
        BEE_VKRESULT_NAME(VK_ERROR_INVALID_EXTERNAL_HANDLE, "Invalid external handle")
        BEE_VKRESULT_NAME(VK_ERROR_SURFACE_LOST_KHR, "Surface lost")
        BEE_VKRESULT_NAME(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "Native window in use")
        BEE_VKRESULT_NAME(VK_SUBOPTIMAL_KHR, "Suboptimal")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_DATE_KHR, "Out of date")
        BEE_VKRESULT_NAME(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "Incompatible display")
        BEE_VKRESULT_NAME(VK_ERROR_VALIDATION_FAILED_EXT, "Validation failed")
        BEE_VKRESULT_NAME(VK_ERROR_INVALID_SHADER_NV, "Invalid shader")
        BEE_VKRESULT_NAME(VK_ERROR_FRAGMENTATION_EXT, "Fragmentation")
        BEE_VKRESULT_NAME(VK_ERROR_NOT_PERMITTED_EXT, "Not permitted")
        BEE_VKRESULT_NAME(VK_RESULT_RANGE_SIZE, "Range size")
        default:
            break;
    }

    return "Unknown error";

#undef BEE_VKRESULT_NAME
}

VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT object_type,
    u64 /*object*/,
    size_t location,
    i32 msg_code,
    const char* layer_prefix,
    const char* msg,
    void* /*user_data*/
)
{
    static constexpr const char* object_names[] =
        {
            "Unknown", // VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT = 0,
            "Instance", // VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT = 1,
            "PhysicalDevice", // VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT = 2,
            "Device", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT = 3,
            "Queue", // VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT = 4,
            "Semaphore", // VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT = 5,
            "CommpiandBuffer", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT = 6,
            "Fence", // VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT = 7,
            "DeviceMemory", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT = 8,
            "Buffer", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT = 9,
            "Image", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT = 10,
            "Event", // VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT = 11,
            "QueryPool", // VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT = 12,
            "BufferView", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT = 13,
            "ImageView", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT = 14,
            "ShaderModule", // VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT = 15,
            "PipelineCache", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT = 16,
            "PipelineLayout", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT = 17,
            "RenderPass", // VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT = 18,
            "Pipeline", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT = 19,
            "DescriptorSetLayout", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT = 20,
            "Sampler", // VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT = 21,
            "DescriptorPool", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT = 22,
            "DescriptorSet", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT = 23,
            "Framebuffer", // VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT = 24,
            "CommandPool", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT = 25,
            "SurfaceKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT = 26,
            "SwapchainKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT = 27,
            "DebugReportCallback", // VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT = 28,
            "DisplayKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT = 29,
            "DisplayModeKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT = 30,
            "ObjectTableNVX", // VK_DEBUG_REPORT_OBJECT_TYPE_OBJECT_TABLE_NVX_EXT = 31,
            "IndirectCommandsLayoutNVX", // VK_DEBUG_REPORT_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX_EXT = 32,
            "ValidationCache", // VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT = 33,
        };

    static const auto get_flag_string = [&]() -> const char*
    {
        if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0)
        {
            return "Info";
        }

        if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
        {
            return "Warning";
        }

        if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0)
        {
            return "Performance warning";
        }

        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
        {
            return "Error";
        }

        if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0)
        {
            return "Debug";
        }

        return "Uknown";
    };

    BEE_ERROR("Vulkan validation", "%s (%s) %s: %d: %s\n", layer_prefix, object_names[object_type],
              get_flag_string(), msg_code, msg);
    log_stack_trace(LogVerbosity::error, 9);
    BEE_DEBUG_BREAK();
    return VK_FALSE;
}


/*
 ****************************************
 *
 * # GPU backend API - implementation
 *
 ****************************************
 */
static struct VulkanBackend
{
    VkInstance                          instance { nullptr };

    i32                                 physical_device_count { 0 };
    VkPhysicalDevice                    physical_devices[BEE_GPU_MAX_PHYSICAL_DEVICES];
    VkPhysicalDeviceProperties          physical_device_properties[BEE_GPU_MAX_PHYSICAL_DEVICES];
    VkPhysicalDeviceMemoryProperties    physical_device_memory_properties[BEE_GPU_MAX_PHYSICAL_DEVICES];

    // There are never more than a few devices active at a time so we don't need to use handle pools.
    // Using a raw array avoids having to do unnecessary bitmask operations or version checking.
    // The difference here being that devices will be allowed to have an ID of zero
    VulkanDevice                        devices[BEE_GPU_MAX_DEVICES];

    static constexpr const char* required_extensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
        , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

#if BEE_OS_WINDOWS == 1
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif // #if BEE_OS_WINDOWS == 1
    };

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    VkDebugReportCallbackEXT debug_report_cb { VK_NULL_HANDLE };

    static constexpr const char* enabled_validation_layers[] =
    {
        "VK_LAYER_LUNARG_standard_validation",  // enables threading, parameter, object memory, core validation
        "VK_LAYER_LUNARG_monitor"               // displays FPS in title bar (maybe useless)
    };
#endif // BEE_CONFIG_ENABLE_ASSERTIONS
} g_backend;


bool gpu_init()
{
    if (BEE_FAIL_F(g_backend.instance == nullptr, "GPU backend is already initialized"))
    {
        return false;
    }

    // Initialize volk extension loader
    const auto volk_result = volkInitialize();
    if (volk_result != VK_SUCCESS)
    {
        log_error("Unable to initialize Vulkan - failed to find the Vulkan loader: %s", vk_result_string(volk_result));
        return false;
    }

    VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = "Bee App";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Bee";
    app_info.engineVersion = VK_MAKE_VERSION(BEE_VERSION_MAJOR, BEE_VERSION_MINOR, BEE_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = static_array_length(g_backend.required_extensions);
    instance_info.ppEnabledExtensionNames = g_backend.required_extensions;
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    instance_info.enabledLayerCount = static_array_length(g_backend.enabled_validation_layers);
    instance_info.ppEnabledLayerNames = g_backend.enabled_validation_layers;
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    BEE_VK_CHECK(vkCreateInstance(&instance_info, nullptr, &g_backend.instance)); // create instance
    volkLoadInstance(g_backend.instance); // load all extensions

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    // Setup debug validation callbacks
    VkDebugReportCallbackCreateInfoEXT debug_cb_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
    debug_cb_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debug_cb_info.pfnCallback = vk_debug_callback;

    BEE_VK_CHECK(vkCreateDebugReportCallbackEXT(g_backend.instance, &debug_cb_info, nullptr, &g_backend.debug_report_cb));
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    // Get all available physical devices up to MAX_PHYSICAL_DEVICES
    u32 device_count = 0;
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend.instance, &device_count, nullptr));
    BEE_ASSERT_F(device_count > 0, "Unable to detect any supported physical graphics devices");

    // Get the physical device info for all available devices regardless of whether they're suitable or not
    device_count = math::min(device_count, BEE_GPU_MAX_PHYSICAL_DEVICES);
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend.instance, &device_count, g_backend.physical_devices));

    // Get info for devices to allow user to select a device later
    for (int pd = 0; pd < device_count; ++pd)
    {
        auto& vk_pd = g_backend.physical_devices[pd];
        vkGetPhysicalDeviceMemoryProperties(vk_pd, &g_backend.physical_device_memory_properties[pd]);
        vkGetPhysicalDeviceProperties(vk_pd, &g_backend.physical_device_properties[pd]);
    }

    g_backend.physical_device_count = sign_cast<i32>(device_count);

    return true;
}

void gpu_destroy()
{
    for (auto& device : g_backend.devices)
    {
        BEE_ASSERT_F(device.handle == nullptr, "All GPU devices must be destroyed before the GPU backend is destroyed");
    }

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    vkDestroyDebugReportCallbackEXT(g_backend.instance, g_backend.debug_report_cb, nullptr);
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    vkDestroyInstance(g_backend.instance, nullptr);
    g_backend.instance = nullptr;
}

i32 gpu_enumerate_physical_devices(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size)
{
    if (dst_buffer == nullptr)
    {
        return g_backend.physical_device_count;
    }

    const auto device_count = math::min(buffer_size, g_backend.physical_device_count);

    for (int pd = 0; pd < device_count; ++pd)
    {
        auto& props = g_backend.physical_device_properties[pd];
        auto& info = dst_buffer[pd];
        str::copy(info.name, PhysicalDeviceInfo::max_name_size, props.deviceName, static_array_length(props.deviceName));

        info.id = pd;
        info.type = convert_device_type(props.deviceType);
        info.vendor = convert_vendor(props.vendorID);

        const auto major = VK_VERSION_MAJOR(props.apiVersion);
        const auto minor = VK_VERSION_MINOR(props.apiVersion);
        const auto patch = VK_VERSION_PATCH(props.apiVersion);
        str::format(info.api_version, static_array_length(info.api_version), "Vulkan %u.%u.%u", major, minor, patch);
    }

    return device_count;
}


} // namespace bee