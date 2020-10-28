/*
 *  VulkanBackend.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define VMA_ASSERT(expr) BEE_ASSERT(expr)
#define VMA_IMPLEMENTATION

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Plugins/Gpu/Gpu.hpp"
#include "Bee/Plugins/VulkanBackend/VulkanDevice.hpp"
#include "Bee/Plugins/VulkanBackend/VulkanConvert.hpp"

namespace bee {


/*
 ************************************************
 *
 * # Vulkan helper functions and debug callbacks
 *
 ************************************************
 */
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
//        BEE_VKRESULT_NAME(VK_RESULT_RANGE_SIZE, "Range size") // removed in newer vulkan versions
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
            "UNKNOWN", // VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT = 0,
            "INSTANCE", // VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT = 1,
            "PHYSICAL_DEVICE", // VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT = 2,
            "DEVICE", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT = 3,
            "QUEUE", // VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT = 4,
            "SEMAPHORE", // VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT = 5,
            "COMMAND_BUFFER", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT = 6,
            "FENCE", // VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT = 7,
            "DEVICE_MEMORY", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT = 8,
            "BUFFER", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT = 9,
            "IMAGE", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT = 10,
            "EVENT", // VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT = 11,
            "QUERY_POOL", // VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT = 12,
            "BUFFER_VIEW", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT = 13,
            "IMAGE_VIEW", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT = 14,
            "SHADER_MODULE", // VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT = 15,
            "PIPELINE_CACHE", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT = 16,
            "PIPELINE_LAYOUT", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT = 17,
            "RENDER_PASS", // VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT = 18,
            "PIPELINE", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT = 19,
            "DESCRIPTOR_SET_LAYOUT", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT = 20,
            "SAMPLER", // VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT = 21,
            "DESCRIPTOR_POOL", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT = 22,
            "DESCRIPTOR_SET", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT = 23,
            "FRAMEBUFFER", // VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT = 24,
            "COMMAND_POOL", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT = 25,
            "SURFACE_KHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT = 26,
            "SWAPCHAIN_KHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT = 27,
            "DEBUG_REPORT_CALLBACK", // VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT = 28,
            "DISPLAY_KHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT = 29,
            "DISPLAY_MODE_KHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT = 30,
            "OBJECT_TABLE_NVX", // VK_DEBUG_REPORT_OBJECT_TYPE_OBJECT_TABLE_NVX_EXT = 31,
            "INDIRECT_COMMANDS_LAYOUT_NVX", // VK_DEBUG_REPORT_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX_EXT = 32,
            "VALIDATION_CACHE", // VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT = 33,
        };

    LogVerbosity verbosity = LogVerbosity::quiet;
    const char* extra_message_type = "";

    if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0)
    {
        verbosity = LogVerbosity::info;
    }
    else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
    {
        verbosity = LogVerbosity::warn;
    }
    else if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0)
    {
        verbosity = LogVerbosity::warn;
        extra_message_type = "[perf]";
    }
    else if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
    {
        verbosity = LogVerbosity::error;
    }
    else if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0)
    {
        verbosity = LogVerbosity::debug;
    }

    log_write(verbosity, "Vulkan%s: %s (%s): %s", extra_message_type, layer_prefix, object_names[object_type], msg);
    log_stack_trace(LogVerbosity::error, 4);
    BEE_DEBUG_BREAK();
    return VK_FALSE;
}

/*
 ******************************************
 *
 * # Vulkan debug markers
 *
 * Not set in release builds - debug only
 *
 ******************************************
 */
#ifdef BEE_DEBUG

void set_vk_object_tag(VulkanDevice* device, VkDebugReportObjectTypeEXT object_type, void* object, size_t tag_size, const void* tag)
{
    if (!device->debug_markers_enabled || tag == nullptr || tag_size == 0 || object == nullptr)
    {
        return;
    }

    VkDebugMarkerObjectTagInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT };
    info.objectType = object_type;
    info.object = (u64)object;
    info.tagName = 0;
    info.tagSize = tag_size;
    info.pTag = tag;
    BEE_VK_CHECK(vkDebugMarkerSetObjectTagEXT(device->handle, &info));
}

void set_vk_object_name(VulkanDevice* device, VkDebugReportObjectTypeEXT object_type, void* object, const char* name)
{
    if (!device->debug_markers_enabled || name == nullptr || object == nullptr)
    {
        return;
    }

    VkDebugMarkerObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
    info.objectType = object_type;
    info.object = (u64)object;
    info.pObjectName = name;
    BEE_VK_CHECK(vkDebugMarkerSetObjectNameEXT(device->handle, &info));
}

#else

void set_vk_object_tag(VulkanDevice* /* device */, VkDebugReportObjectTypeEXT /* object_type */, void* /* object */, size_t /* tag_size */, const void* /* tag */)
{
    // no-op
}

void set_vk_object_name(VulkanDevice* /* device */, VkDebugReportObjectTypeEXT /* object_type */, void* /* object */, const char* /* name */)
{
    // no-op
}

#endif // BEE_DEBUG


/*
 ************************************
 *
 * VulkanBackend - implementation
 *
 ************************************
 */
static VulkanBackend* g_backend { nullptr };


#define BEE_GPU_VALIDATE_BACKEND() BEE_ASSERT_F(g_backend->instance != nullptr, "GPU backend has not been initialized")

VulkanDevice& validate_device(const DeviceHandle& device)
{
    BEE_GPU_VALIDATE_BACKEND();
    BEE_ASSERT_F(
        device.id < BEE_GPU_MAX_DEVICES && g_backend->devices[device.id].handle != VK_NULL_HANDLE,
        "GPU device has an invalid ID or is destroyed/uninitialized"
    );
    return g_backend->devices[device.id];
}

bool init()
{
    if (BEE_FAIL_F(g_backend->instance == nullptr, "GPU backend is already initialized"))
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

    VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr };
    app_info.pApplicationName = "Bee App";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Bee";
    app_info.engineVersion = VK_MAKE_VERSION(BEE_VERSION_MAJOR, BEE_VERSION_MINOR, BEE_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr };
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = static_array_length(VulkanBackend::required_extensions);
    instance_info.ppEnabledExtensionNames = VulkanBackend::required_extensions;
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    instance_info.enabledLayerCount = static_array_length(VulkanBackend::enabled_validation_layers);
    instance_info.ppEnabledLayerNames = VulkanBackend::enabled_validation_layers;
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    BEE_VK_CHECK(vkCreateInstance(&instance_info, nullptr, &g_backend->instance)); // create instance
    volkLoadInstance(g_backend->instance); // load all extensions

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    // Setup debug validation callbacks
    VkDebugReportCallbackCreateInfoEXT debug_cb_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
    debug_cb_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debug_cb_info.pfnCallback = vk_debug_callback;

    BEE_VK_CHECK(vkCreateDebugReportCallbackEXT(g_backend->instance, &debug_cb_info, nullptr, &g_backend->debug_report_cb));
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    // Get all available physical devices up to MAX_PHYSICAL_DEVICES
    u32 device_count = 0;
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend->instance, &device_count, nullptr));
    BEE_ASSERT_F(device_count > 0, "Unable to detect any supported physical graphics devices");

    // Get the physical device info for all available devices regardless of whether they're suitable or not
    device_count = math::min(device_count, BEE_GPU_MAX_PHYSICAL_DEVICES);
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend->instance, &device_count, g_backend->physical_devices));

    // Get info for devices to allow user to select a device later
    for (u32 pd = 0; pd < device_count; ++pd)
    {
        auto& vk_pd = g_backend->physical_devices[pd];
        vkGetPhysicalDeviceMemoryProperties(vk_pd, &g_backend->physical_device_memory_properties[pd]);
        vkGetPhysicalDeviceProperties(vk_pd, &g_backend->physical_device_properties[pd]);
    }

    g_backend->physical_device_count = sign_cast<i32>(device_count);

    return true;
}

void destroy()
{
    for (auto& device : g_backend->devices)
    {
        BEE_ASSERT_F(device.handle == nullptr, "All GPU devices must be destroyed before the GPU backend is destroyed");
    }

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    vkDestroyDebugReportCallbackEXT(g_backend->instance, g_backend->debug_report_cb, nullptr);
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    vkDestroyInstance(g_backend->instance, nullptr);
    g_backend->instance = VK_NULL_HANDLE;
}

i32 enumerate_physical_devices(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size)
{
    if (dst_buffer == nullptr)
    {
        return g_backend->physical_device_count;
    }

    const auto device_count = math::min(buffer_size, g_backend->physical_device_count);

    for (int pd = 0; pd < device_count; ++pd)
    {
        auto& props = g_backend->physical_device_properties[pd];
        auto& info = dst_buffer[pd];
        str::copy(info.name, PhysicalDeviceInfo::max_name_size, props.deviceName, static_array_length(props.deviceName));

        info.id = pd;
        info.type = convert_device_type(props.deviceType);
        info.vendor = convert_vendor(props.vendorID);

        const auto major = VK_VERSION_MAJOR(props.apiVersion);
        const auto minor = VK_VERSION_MINOR(props.apiVersion);
        const auto patch = VK_VERSION_PATCH(props.apiVersion);
        str::format_buffer(info.api_version, static_array_length(info.api_version), "Vulkan %u.%u.%u", major, minor, patch);
    }

    return device_count;
}

/*
 ******************************************
 *
 * # Device objects
 *
 ******************************************
 */
DeviceHandle create_device(const DeviceCreateInfo& create_info)
{
    BEE_GPU_VALIDATE_BACKEND();

    if (g_backend->instance == VK_NULL_HANDLE)
    {
        log_error("Failed to create GPU device: Vulkan instance was VK_NULL_HANDLE");
        return DeviceHandle{};
    }

    const auto is_valid_physical_device_id = create_info.physical_device_id >= 0
        && create_info.physical_device_id < g_backend->physical_device_count;
    if (BEE_FAIL_F(is_valid_physical_device_id, "Invalid physical device ID specified in `DeviceCreateInfo`"))
    {
        return DeviceHandle{};
    }

    int device_idx = find_index_if(g_backend->devices, [](const VulkanDevice& d)
    {
        return d.handle == VK_NULL_HANDLE;
    });
    if (BEE_FAIL_F(device_idx >= 0, "Cannot create a new GPU device: Allocated devices has reached BEE_GPU_MAX_DEVICES"))
    {
        return DeviceHandle{};
    }

    auto* physical_device = g_backend->physical_devices[create_info.physical_device_id];
    auto& device = g_backend->devices[device_idx];
    device.physical_device = physical_device;
    device.debug_markers_enabled = false;

    // Query the amount of extensions supported by the GPU
#ifdef BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED
    u32 ext_count = 0;
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, nullptr));

    // Get the actual extension properties for the GPU
    auto supported_extensions = FixedArray<VkExtensionProperties>::with_size(sign_cast<int>(ext_count), temp_allocator());
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, supported_extensions.data()));

    DynamicArray<const char*> device_extensions(temp_allocator());

    for (const auto* ext : VulkanBackend::device_extensions)
    {
        const auto found_index = find_index_if(supported_extensions, [&](const VkExtensionProperties& prop)
        {
            return str::compare(ext, prop.extensionName) == 0;
        });

        if (found_index < 0)
        {
            log_error("Vulkan: required extension \"%s\" is not supported", ext);
        }
        else
        {
            device_extensions.push_back(ext);

            if (str::compare(ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
            {
                device.debug_markers_enabled = true;
            }
        }
    }
#else
    DynamicArray<const char*> device_extensions(temp_allocator());
    for (const auto& ext : VulkanBackend::device_extensions)
    {
        device_extensions.push_back(ext);
    }
#endif // BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED

    // Find all available queue families and store in device data for later use
    u32 available_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &available_queue_families, nullptr);
    available_queue_families = math::min(available_queue_families, vk_max_queues);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &available_queue_families, device.queue_family_properties);

    memset(device.queues, 0, sizeof(VulkanQueue) * available_queue_families);

    /*
     * This function looks for a matching queue that has the lowest functionality available to allow using
     * it in the most specialized way possible
     */
    auto find_queue_index = [](const VulkanDevice& device, const VkQueueFlags type) -> u32
    {
        int lowest_count = limits::max<int>();
        int best_so_far = limits::max<u32>();

        for (u32 q = 0; q < vk_max_queues; ++q)
        {
            if ((device.queue_family_properties[q].queueFlags & type) == 0)
            {
                continue;
            }

            int supported_count = 1;
            for_each_flag(device.queue_family_properties[q].queueFlags, [&](const VkQueueFlags flag)
            {
                ++supported_count;
            });

            if (supported_count < lowest_count)
            {
                lowest_count = supported_count;
                best_so_far = q;
            }
        }

        return best_so_far;
    };

    /*
     * We want the graphics queue to also double as a combined queue for gfx, compute, & transfer so here
     * we search for a queue matching GRAPHICS_BIT | COMPUTE_BIT because according to the spec
     * (Section 4.1 in the discussion of VkQueueFlagBits):
     *
     * 'If an implementation exposes any queue family that supports graphics operations, at least
     *  one queue family of at least one physical device exposed by the implementation must support
     *  **both** graphics and compute operations'
     *
     * Therefore, we can safely assume that if graphics is supported so is a generic graphics/compute
     * queue. Also any queue that defines graphics or compute operations also implicitly guarantees
     * transfer operations - so all of these calls should return valid queue indexes
     */
    device.graphics_queue.index = find_queue_index(device, VK_QUEUE_GRAPHICS_BIT);
    device.transfer_queue.index = find_queue_index(device, VK_QUEUE_TRANSFER_BIT);
    device.compute_queue.index = find_queue_index(device, VK_QUEUE_COMPUTE_BIT);

    BEE_ASSERT(device.graphics_queue.index < VulkanQueue::invalid_queue_index);

    int queue_info_indices[vk_max_queues];
    VkDeviceQueueCreateInfo queue_infos[vk_max_queues];

    memset(queue_infos, 0, sizeof(VkDeviceQueueCreateInfo) * vk_max_queues);
    memset(queue_info_indices, -1, sizeof(int) * vk_max_queues);

    u32 queue_family_count = 0;
    float queue_priorities[] = { 1.0f, 1.0f, 1.0f }; // in case all three queues are in the one family

    for (const auto& queue : device.queues)
    {
        if (queue_info_indices[queue.index] < 0)
        {
            queue_info_indices[queue.index] = queue_family_count++;

            auto& info = queue_infos[queue_info_indices[queue.index]];
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.pNext = nullptr;
            info.flags = 0;
            info.queueFamilyIndex = queue.index;
            info.pQueuePriorities = queue_priorities;
            info.queueCount = 0;
        }

        ++queue_infos[queue_info_indices[queue.index]].queueCount;
    }

    VkPhysicalDeviceFeatures supported_features{};
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

    VkPhysicalDeviceFeatures enabled_features{};
    memset(&enabled_features, 0, sizeof(VkPhysicalDeviceFeatures));

#define BEE_ENABLE_FEATURE(vk_feature, bee_feature)                                                                 \
    BEE_BEGIN_MACRO_BLOCK                                                                                           \
        enabled_features.vk_feature = vkbool_cast(create_info.bee_feature && supported_features.vk_feature);        \
        if (create_info.bee_feature && vkbool_cast(enabled_features.vk_feature))                                    \
        {                                                                                                           \
            log_error(#bee_feature " is not a feature supported by the specified physical GPU device");             \
        }                                                                                                           \
    BEE_END_MACRO_BLOCK


    // Enable requested features if available
    BEE_ENABLE_FEATURE(depthClamp, enable_depth_clamp);
    BEE_ENABLE_FEATURE(sampleRateShading, enable_sample_rate_shading);
    BEE_ENABLE_FEATURE(samplerAnisotropy, enable_sampler_anisotropy);

#undef BEE_ENABLE_FEATURE

    VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_info.queueCreateInfoCount = queue_family_count;
    device_info.pQueueCreateInfos = queue_infos;
    device_info.enabledExtensionCount = sign_cast<u32>(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.pEnabledFeatures = &enabled_features;
    BEE_VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device.handle));

    volkLoadDevice(device.handle); // register device with volk and load extensions

    // Retrieve the actual queue object handles
    for (int i = 0; i < static_array_length(device.queues); ++i)
    {
        vkGetDeviceQueue(device.handle, device.queues[i].index, 0, &device.queues[i].handle);
    }

    VmaVulkanFunctions vma_functions{};
    vma_functions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vma_functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vma_functions.vkAllocateMemory = vkAllocateMemory;
    vma_functions.vkFreeMemory = vkFreeMemory;
    vma_functions.vkMapMemory = vkMapMemory;
    vma_functions.vkUnmapMemory = vkUnmapMemory;
    vma_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vma_functions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vma_functions.vkBindBufferMemory = vkBindBufferMemory;
    vma_functions.vkBindImageMemory = vkBindImageMemory;
    vma_functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vma_functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vma_functions.vkCreateBuffer = vkCreateBuffer;
    vma_functions.vkDestroyBuffer = vkDestroyBuffer;
    vma_functions.vkCreateImage = vkCreateImage;
    vma_functions.vkDestroyImage = vkDestroyImage;
    vma_functions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION
    vma_functions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
    vma_functions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
#endif
#if VMA_BIND_MEMORY2
    vma_functions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
    vma_functions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
#endif

    VmaAllocatorCreateInfo vma_info{};
    vma_info.device = device.handle;
    vma_info.physicalDevice = physical_device;
    vma_info.pVulkanFunctions = &vma_functions;

    BEE_VK_CHECK(vmaCreateAllocator(&vma_info, &device.vma_allocator));

    // initialize caches
    device.descriptor_set_layout_cache.init(&device, create_descriptor_set_layout, destroy_descriptor_set_layout);
    device.pipeline_layout_cache.init(&device, create_pipeline_layout, destroy_pipeline_layout);
    device.framebuffer_cache.init(&device, create_framebuffer, destroy_framebuffer);

    // initialize thread-local data
    device.thread_data.resize(job_system_worker_count());

    auto cmd_pool_info = VkCommandPoolCreateInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    cmd_pool_info.flags = 0;
    cmd_pool_info.queueFamilyIndex = device.graphics_queue.index;

    for (int i = 0; i < device.thread_data.size(); ++i)
    {
        auto& thread = device.thread_data[i];
        thread.index = i;

        // initialize the general purpose allocator and resource tables
        new (&thread.allocator) ChunkAllocator(get_page_size(), 64, 0);

        for (auto& table : thread.resource_tables)
        {
            table.init(i);
        }

        // Initialize the staging buffers
        thread.staging.init(&device, &device.transfer_queue, device.vma_allocator);

        // Create command pool per thread per frame
        for (int frame = 0; frame < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            BEE_VK_CHECK(vkCreateCommandPool(device.handle, &cmd_pool_info, nullptr, &thread.command_pool[frame].handle));
        }

        // Setup queue submissions
        for (int queue = 0; queue < vk_max_queues; ++queue)
        {
            thread.queue_submissions[queue].queue = queue;
        }
    }

    return DeviceHandle(sign_cast<u32>(device_idx));
}

void destroy_device(const DeviceHandle& handle)
{
    auto& device = validate_device(handle); 

    // Destroy cached objects
    device.descriptor_set_layout_cache.destroy();
    device.pipeline_layout_cache.destroy();
    device.framebuffer_cache.destroy();

    // Destroy the vulkan-related thread data
    for (auto& thread : device.thread_data)
    {
        thread.staging.destroy();

        for (auto& descriptor_cache : thread.dynamic_descriptor_pools)
        {
            descriptor_cache.destroy(device.handle);
        }
    }

    vmaDestroyAllocator(device.vma_allocator);
    vkDestroyDevice(device.handle, nullptr);
    destruct(&device);

    device.physical_device = VK_NULL_HANDLE;
    device.handle = VK_NULL_HANDLE;
    device.vma_allocator = VK_NULL_HANDLE;
}

void device_wait(const DeviceHandle& handle)
{
    vkDeviceWaitIdle(validate_device(handle).handle);
}

void VulkanQueueSubmit::reset()
{
    memset(&info, 0, sizeof(VkSubmitInfo));
    cmd_buffers.clear();
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
}

void VulkanQueueSubmit::add(CommandBuffer* cmd)
{
    cmd->state = CommandBufferState::pending;
    cmd_buffers.push_back(cmd->handle);
}

void VulkanQueueSubmit::submit(VulkanDevice* device, VkFence fence)
{
    if (cmd_buffers.empty())
    {
        return;
    }

    info.commandBufferCount = sign_cast<u32>(cmd_buffers.size());
    info.pCommandBuffers = cmd_buffers.data();
    device->queues[queue].submit(info, fence, device);
}

void VulkanQueue::submit(const VkSubmitInfo& submit_info, VkFence fence, VulkanDevice* device) const
{
    /*
   * vkQueueSubmit can access a queue across multiple threads as long as it's externally
   * synchronized i.e. with a mutex
   * see: Vulkan Spec - 2.6. Threading Behavior
   */
    scoped_recursive_lock_t lock(device->per_queue_mutex[index]);
    BEE_VK_CHECK(vkQueueSubmit(handle, 1, &submit_info, fence));
}

VkResult VulkanQueue::present(const VkPresentInfoKHR* present_info, VulkanDevice* device) const
{
    /*
     * vkQueuePresentKHR can access a queue across multiple threads as long as it's externally
     * synchronized i.e. with a mutex
     * see: Vulkan Spec - 2.6. Threading Behavior
     */
    scoped_recursive_lock_t lock(device->per_queue_mutex[index]);
    return vkQueuePresentKHR(handle, present_info);
}

/*
 ***********************
 *
 * Swapchain management
 *
 ***********************
 */
bool recreate_swapchain(VulkanDevice* device, VulkanSwapchain* swapchain, const i32 swapchain_index, const SwapchainCreateInfo& create_info)
{
    // Create a surface and query its capabilities
    auto* surface = swapchain->surface;

    if (surface != VK_NULL_HANDLE)
    {
        // check for lost surface with recreated swapchain
        VkSurfaceCapabilitiesKHR surface_caps{};
        const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, surface, &surface_caps);
        if (result == VK_ERROR_SURFACE_LOST_KHR)
        {
            // destroy the existing swapchain linked to the surface as well as the old surface object
            BEE_ASSERT(swapchain->handle != VK_NULL_HANDLE);

            vkDestroySwapchainKHR(device->handle, swapchain->handle, nullptr);
            vkDestroySurfaceKHR(g_backend->instance, swapchain->surface, nullptr);

            swapchain->handle = VK_NULL_HANDLE;
            swapchain->surface = VK_NULL_HANDLE;
            surface = VK_NULL_HANDLE;
        }
    }

    if (surface == VK_NULL_HANDLE)
    {
        surface = vk_create_wsi_surface(g_backend->instance, create_info.window);
        BEE_ASSERT(surface != VK_NULL_HANDLE);
    }

    /*
     * If we've never found the present queue for the device we have to do it here rather than in create_device
     * as it requires a valid surface to query.
     */
    if (device->present_queue == VulkanQueue::invalid_queue_index)
    {
        // Prefers graphics/present combined queue over other combinations - first queue is always the graphics queue
        VkBool32 supports_present = VK_FALSE;
        for (const auto& queue : device->queues)
        {
            BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
                device->physical_device,
                device->graphics_queue.index,
                surface,
                &supports_present
            ));

            if (supports_present == VK_TRUE)
            {
                device->present_queue = queue.index;
                break;
            }
        }
    }

    // Get the surface capabilities and ensure it supports all the things we need
    VkSurfaceCapabilitiesKHR surface_caps{};
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, surface, &surface_caps));

    // Get supported formats
    u32 format_count = 0;
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &format_count, nullptr));
    auto formats = FixedArray<VkSurfaceFormatKHR>::with_size(sign_cast<i32>(format_count), temp_allocator());
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &format_count, formats.data()));

    // Get supported present modes
    u32 present_mode_count = VK_PRESENT_MODE_FIFO_RELAXED_KHR + 1;
    VkPresentModeKHR present_modes[VK_PRESENT_MODE_FIFO_RELAXED_KHR + 1];
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, surface, &present_mode_count, present_modes));

    // Choose an appropriate image count - try and get MAX_FRAMES_IN_FLIGHT first, otherwise fit in range of minImageCount -> maxImageCount
    auto image_count = math::min(math::max(BEE_GPU_MAX_FRAMES_IN_FLIGHT, surface_caps.minImageCount), surface_caps.maxImageCount);

    // Select a vk_handle image format - first try and get the format requested in create_info otherwise just choose first available format
    const auto desired_format = convert_pixel_format(create_info.texture_format);
    const auto desired_format_idx = find_index_if(formats, [&](const VkSurfaceFormatKHR& fmt)
    {
        return fmt.format == desired_format;
    });
    auto selected_format = formats[0];
    if (desired_format_idx >= 0)
    {
        selected_format = formats[desired_format_idx];
    }

    /*
     * Find a valid present mode for the VSync mode chosen.
     * Prefer mailbox for when VSync is off as it waits for the blank interval but
     * replaces the image at the back of the queue instead of causing tearing like IMMEDIATE_KHR does
     */
    auto present_mode = VK_PRESENT_MODE_FIFO_KHR; // vsync on
    if (!create_info.vsync)
    {
        const auto supports_mailbox = find_index_if(present_modes, [](const VkPresentModeKHR& mode)
        {
            return mode == VK_PRESENT_MODE_MAILBOX_KHR;
        }) >= 0;
        present_mode = supports_mailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    const auto& requested_extent = create_info.texture_extent;
    Extent actual_extent{};
    actual_extent.width = math::min(math::max(requested_extent.width, surface_caps.minImageExtent.width), surface_caps.maxImageExtent.width);
    actual_extent.height = math::min(math::max(requested_extent.height, surface_caps.minImageExtent.height), surface_caps.maxImageExtent.height);

    // Create the vk_handle
    VkSwapchainCreateInfoKHR swapchain_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_info.flags                 = 0;
    swapchain_info.surface               = surface;
    swapchain_info.minImageCount         = image_count;
    swapchain_info.imageFormat           = selected_format.format;
    swapchain_info.imageColorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent.width     = actual_extent.width;
    swapchain_info.imageExtent.height    = actual_extent.height;
    swapchain_info.imageArrayLayers      = create_info.texture_array_layers;
    swapchain_info.imageUsage            = decode_image_usage(create_info.texture_usage);
    swapchain_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices   = nullptr;
    swapchain_info.preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // no pre-transform
    swapchain_info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore surface alpha channel
    swapchain_info.presentMode           = present_mode;
    swapchain_info.clipped               = VK_TRUE; // allows optimal presentation of pixels clipped in the surface by other OS windows etc.
    swapchain_info.oldSwapchain          = swapchain->handle != VK_NULL_HANDLE ? swapchain->handle : VK_NULL_HANDLE;

    VkSwapchainKHR vk_handle = VK_NULL_HANDLE;
    BEE_VK_CHECK(vkCreateSwapchainKHR(device->handle, &swapchain_info, nullptr, &vk_handle));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, vk_handle, create_info.debug_name);
    }

    // destroy the old swapchain after transitioning it into the new one
    if (swapchain_info.oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device->handle, swapchain_info.oldSwapchain, nullptr);

        // destroy the old semaphores
        for (int frame_idx = 0; frame_idx < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame_idx)
        {
            vkDestroySemaphore(device->handle, swapchain->acquire_semaphore[frame_idx], nullptr);
            vkDestroySemaphore(device->handle, swapchain->render_semaphore[frame_idx], nullptr);
        }
    }

    // Setup the swapchain images
    u32 swapchain_image_count = 0;
    BEE_VK_CHECK(vkGetSwapchainImagesKHR(device->handle, vk_handle, &swapchain_image_count, nullptr));
    auto swapchain_images = FixedArray<VkImage>::with_size(sign_cast<i32>(swapchain_image_count), temp_allocator());
    BEE_VK_CHECK(vkGetSwapchainImagesKHR(device->handle, vk_handle, &swapchain_image_count, swapchain_images.data()));

    swapchain->handle       = vk_handle;
    swapchain->surface      = surface;
    swapchain->images       = FixedArray<TextureHandle>::with_size(image_count);
    swapchain->image_views  = FixedArray<TextureViewHandle>::with_size(image_count);
    swapchain->create_info  = create_info;
    swapchain->create_info.texture_extent = actual_extent; // fixup the extent in the stored create info

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, vk_handle, create_info.debug_name);
    }
    /*
     * Insert a texture handle for each of the swapchain images to use with external code and create a texture view
     * for each one
     */
    TextureViewCreateInfo view_info;
    view_info.type                  = TextureType::tex2d;
    view_info.format                = create_info.texture_format;
    view_info.mip_level_count       = 1;
    view_info.mip_level_offset      = 0;
    view_info.array_element_offset  = 0;
    view_info.array_element_count   = 1;

    auto& thread = device->get_thread();

    for (int si = 0; si < swapchain_images.size(); ++si)
    {
        if (!swapchain->images[si].is_valid())
        {
            auto* texture = BEE_NEW(thread.allocator, VulkanTexture);
            texture->swapchain           = swapchain_index;
            texture->width               = swapchain_info.imageExtent.width;
            texture->height              = swapchain_info.imageExtent.height;
            texture->layers              = swapchain_info.imageArrayLayers;
            texture->levels              = 1;
            texture->samples             = VK_SAMPLE_COUNT_1_BIT;
            texture->format              = create_info.texture_format;
            texture->handle              = swapchain_images[si];
            set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture->handle, "Swapchain image");

            swapchain->images[si] = thread.textures.add(texture);
        }
        else
        {
            // TODO(Jacob): do we only need to reassign the texture params when recreating a swapchain?
            auto* texture = device->textures_get(swapchain->images[si]);
            texture->width               = swapchain_info.imageExtent.width;
            texture->height              = swapchain_info.imageExtent.height;
            texture->layers              = swapchain_info.imageArrayLayers;
            texture->format              = create_info.texture_format;
            texture->handle              = swapchain_images[si];
        }

        // Create a texture view as well
        view_info.texture               = swapchain->images[si];
        view_info.debug_name            = "Swapchain texture view";

        if (!swapchain->image_views[si].is_valid())
        {
            auto* texture_view              = BEE_NEW(thread.allocator, VulkanTextureView);
            texture_view->swapchain         = swapchain_index;

            const auto success              = create_texture_view_internal(device, view_info, texture_view);
            BEE_ASSERT_F(success, "Failed to initialize swapchain image views");

            swapchain->image_views[si]      = thread.texture_views.add(texture_view);
        }
        else
        {
            // Recreate the image view if the swapchain is existing
            auto* texture_view = device->texture_views_get(swapchain->image_views[si]);
            vkDestroyImageView(device->handle, texture_view->handle, nullptr);

            const auto success              = create_texture_view_internal(device, view_info, texture_view);
            BEE_ASSERT_F(success, "Failed to initialize swapchain image views");
        }
    }

    // if this is a new swapchain we need to create new semaphores
    // Create image available and render finished semaphores
    VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    sem_info.flags = 0;

    for (int frame_idx = 0; frame_idx < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame_idx)
    {
        BEE_VK_CHECK(vkCreateSemaphore(device->handle, &sem_info, nullptr, &swapchain->acquire_semaphore[frame_idx]));
        BEE_VK_CHECK(vkCreateSemaphore(device->handle, &sem_info, nullptr, &swapchain->render_semaphore[frame_idx]));
    }

    return true;
}

SwapchainHandle create_swapchain(const DeviceHandle& device_handle, const SwapchainCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    const auto swapchain_index = find_index_if(device.swapchains, [&](const VulkanSwapchain& s)
    {
        return s.handle == VK_NULL_HANDLE;
    });

    if (swapchain_index < 0)
    {
        return SwapchainHandle{};
    }

    if (!recreate_swapchain(&device, &device.swapchains[swapchain_index], swapchain_index, create_info))
    {
        return SwapchainHandle{};
    }

    return SwapchainHandle(swapchain_index);
}

void destroy_swapchain(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];

    BEE_VK_CHECK(vkDeviceWaitIdle(device.handle));

    for (int i = 0; i < swapchain.images.size(); ++i)
    {
        if (swapchain.image_views[i].is_valid())
        {
            auto handle = swapchain.image_views[i];
            auto& thread = device.get_thread(handle);
            auto* texture_view = static_cast<VulkanTextureView*>(thread.texture_views.remove(handle));
            vkDestroyImageView(device.handle, texture_view->handle, nullptr);
            BEE_DELETE(device.get_thread(handle).allocator, texture_view);
        }

        if (swapchain.images[i].is_valid())
        {
            auto handle = swapchain.images[i];
            auto& thread = device.get_thread(handle);
            auto* texture = static_cast<VulkanTexture*>(thread.textures.remove(handle));
            vkDestroyImage(device.handle, texture->handle, nullptr);
            BEE_DELETE(device.get_thread(handle).allocator, texture);
        }

        if (swapchain.acquire_semaphore[i] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device.handle, swapchain.acquire_semaphore[i], nullptr);
        }

        if (swapchain.render_semaphore[i] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device.handle, swapchain.render_semaphore[i], nullptr);
        }
    }

    vkDestroySwapchainKHR(device.handle, swapchain.handle, nullptr);
    vkDestroySurfaceKHR(g_backend->instance, swapchain.surface, nullptr);

    swapchain.handle = VK_NULL_HANDLE;
}

TextureHandle acquire_swapchain_texture(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];
    /*
     * vkAcquireNextImageKHR can access a swapchain across multiple threads as long as it's externally
     * synchronized
     * see: Vulkan Spec - 2.6. Threading Behavior
     */
    scoped_recursive_lock_t lock(swapchain.mutex);

    if (swapchain.pending_image_acquire)
    {
        const auto result = vkAcquireNextImageKHR(
            device.handle,
            swapchain.handle,
            limits::max<u64>(),
            swapchain.acquire_semaphore[swapchain.present_index],
            VK_NULL_HANDLE,
            &swapchain.current_image // get the next image index
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreate_swapchain(&device, &swapchain, swapchain_handle.id, swapchain.create_info);
            acquire_swapchain_texture(device_handle, swapchain_handle);
        }
        else
        {
            BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));
        }

        swapchain.pending_image_acquire = false;
    }

    return swapchain.images[swapchain.current_image];
}

TextureViewHandle get_swapchain_texture_view(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];

    acquire_swapchain_texture(device_handle, swapchain_handle);

    return swapchain.image_views[swapchain.current_image];
}

Extent get_swapchain_extent(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];
    return swapchain.create_info.texture_extent;
}

PixelFormat get_swapchain_texture_format(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];
    return swapchain.create_info.texture_format;
}

PixelFormat get_texture_format(const DeviceHandle& device_handle, const TextureHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(handle);
    auto* texture = static_cast<VulkanTexture*>(thread.textures.get(handle));
    return texture->format;
}

void submit(const DeviceHandle& device_handle, const SubmitInfo& info)
{
    static constexpr VkPipelineStageFlags swapchain_wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (info.command_buffer_count == 0)
    {
        log_warning("GPU warning: created a submit request with 0 command buffers");
        return;
    }

    BEE_ASSERT_F(info.command_buffers != nullptr, "`command_buffers` must point to an array of `command_buffer_count` GpuCommandBuffer pointers");

    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    for (auto& submit : thread.queue_submissions)
    {
        submit.reset();
    }

    // Gather all the command buffers into per-queue submissions
    for (u32 i = 0; i < info.command_buffer_count; ++i)
    {
        auto* cmd = info.command_buffers[i];
        auto& submission = thread.queue_submissions[cmd->queue->index];

        // we have to add a semaphore if the command buffer is targeting the swapchain
        if (cmd->target_swapchain >= 0)
        {
            auto& swapchain = device.swapchains[cmd->target_swapchain];

            if (BEE_FAIL_F(!swapchain.pending_image_acquire, "Swapchain cannot be rendered to without first acquiring its current texture"))
            {
                return;
            }

            submission.info.waitSemaphoreCount = 1;
            submission.info.pWaitSemaphores = &swapchain.acquire_semaphore[swapchain.present_index];
            submission.info.pWaitDstStageMask = &swapchain_wait_stage;
            submission.info.signalSemaphoreCount = 1;
            submission.info.pSignalSemaphores = &swapchain.render_semaphore[swapchain.present_index];
        }

        submission.add(cmd);
    }

    VkFence submit_fence = VK_NULL_HANDLE;
    {
        scoped_recursive_lock_t lock(device.fence_mutex);
        if (device.free_submit_fences[device.current_frame].empty())
        {
            VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
            fence_info.flags = 0;
            BEE_VK_CHECK(vkCreateFence(device.handle, &fence_info, nullptr, &submit_fence));
        }
        else
        {
            submit_fence = device.free_submit_fences[device.current_frame].back();
            device.free_submit_fences[device.current_frame].pop_back();
        }
    }

    device.used_submit_fences[device.current_frame].push_back(submit_fence);

    for (auto& submission : thread.queue_submissions)
    {
        submission.submit(&device, submit_fence);
    }

//    const VkPipelineStageFlags*    vk_info.pWaitDstStageMask;
//    uint32_t                       vk_info.commandBufferCount;
//    const VkCommandBuffer*         vk_info.pCommandBuffers;
//    uint32_t                       vk_info.signalSemaphoreCount;
//    const VkSemaphore*             vk_info.pSignalSemaphores;
};

void present(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle.id];

    // ensure the swapchain has acquired its next image before presenting if not already acquired
    if (BEE_FAIL_F(!swapchain.pending_image_acquire, "GPU: it's not valid to present a swapchain before acquiring its next texture index"))
    {
        return;
    }

    auto info = VkPresentInfoKHR{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &swapchain.render_semaphore[swapchain.present_index];
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain.handle;
    info.pImageIndices = &swapchain.current_image;
    info.pResults = nullptr;

    const auto result = device.graphics_queue.present(&info, &device);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreate_swapchain(&device, &swapchain, swapchain_handle.id, swapchain.create_info);
    }
    else
    {
        BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));
    }

    // prepare to acquire next image in the next present
    swapchain.pending_image_acquire = true;
    swapchain.present_index = (swapchain.present_index + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;
}

void commit_frame(const DeviceHandle& device_handle)
{
    auto& device = validate_device(device_handle);

    scoped_recursive_lock_t lock(device.device_mutex);

    // process all the pending deletes now that we have exclusive access
    for (auto& thread : device.thread_data)
    {
        // delete the pending CPU memory allocations made via the threads chunk allocator
        u8* delete_node = thread.delete_list;
        while (delete_node != nullptr)
        {
            // we can use free here because add_pending_delete already destructed the object
            thread.delete_list = reinterpret_cast<u8**>(thread.delete_list)[0];
            BEE_FREE(thread.allocator, delete_node);
            delete_node = thread.delete_list;
        }

        // free and then delete the pending static descriptor sets
        VulkanResourceBinding* binding_node = thread.static_resource_binding_pending_deletes;
        while (binding_node != nullptr)
        {
            auto* next = binding_node->next;
            --binding_node->pool->allocated_sets;
            vkFreeDescriptorSets(device.handle, binding_node->pool->handle, 1, &binding_node->set);
            BEE_DELETE(binding_node->pool->thread->allocator, binding_node);
            binding_node = next;
        }

        thread.delete_list = nullptr;
        thread.static_resource_binding_pending_deletes = nullptr;
    }

    device.descriptor_set_layout_cache.sync();
    device.pipeline_layout_cache.sync();
    device.framebuffer_cache.sync();
    device.current_frame = (device.current_frame + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;

    if (!device.used_submit_fences[device.current_frame].empty())
    {
        // Wait on all the executing submissions from the new frame
        const auto wait_result = vkWaitForFences(
            device.handle,
            device.used_submit_fences[device.current_frame].size(),
            device.used_submit_fences[device.current_frame].data(),
            VK_TRUE,
            limits::max<u64>()
        );
        BEE_ASSERT_F(wait_result == VK_SUCCESS || wait_result == VK_TIMEOUT, "Vulkan: %s", vk_result_string(wait_result));

        BEE_VK_CHECK(vkResetFences(
            device.handle,
            device.used_submit_fences[device.current_frame].size(),
            device.used_submit_fences[device.current_frame].data()
        ));

        // Return the submit fences to the free pool
        for (auto& fence : device.used_submit_fences[device.current_frame])
        {
            device.free_submit_fences[device.current_frame].push_back(fence);
        }
        device.used_submit_fences[device.current_frame].clear();
    }

    // Reset all the per-thread command pools for the current frame
    for (auto& thread : device.thread_data)
    {
        // Handle all the deferred removals
        for (auto& table : thread.resource_tables)
        {
            table.flush_removed();
        }

        auto& command_pool = thread.command_pool[device.current_frame];

        // Reset the threads command pool and start again with 0 in-use command buffers
        BEE_VK_CHECK(vkResetCommandPool(device.handle, command_pool.handle, 0));
        thread.command_pool[device.current_frame].command_buffer_count = 0;

        // Destroy pending descriptor pool deletes leftover from resizes
        thread.dynamic_descriptor_pools[device.current_frame].clear_pending(device.handle);
        thread.dynamic_descriptor_pools[device.current_frame].reset(device.handle);
    }
}

i32 get_current_frame(const DeviceHandle& device_handle)
{
    return validate_device(device_handle).current_frame;
}

/*
 ********************
 *
 * Resource commands
 *
 ********************
 */
CommandBuffer* allocate_command_buffer(const DeviceHandle& device_handle, const QueueType queue)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto& cmd_pool = thread.command_pool[device.current_frame];

    if (cmd_pool.command_buffer_count >= static_array_length(cmd_pool.command_buffers))
    {
        log_error("Failed to create command buffer: Command pool for thread %d exhausted", thread.index);
        return nullptr;
    }

    const i32 cmd_buffer_index = cmd_pool.command_buffer_count++;
    auto& cmd_buffer = cmd_pool.command_buffers[cmd_buffer_index];

    if (cmd_buffer.handle == VK_NULL_HANDLE)
    {
        VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        alloc_info.commandPool = cmd_pool.handle;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        BEE_VK_CHECK(vkAllocateCommandBuffers(device.handle, &alloc_info, &cmd_buffer.handle));
    }

    switch (queue)
    {
        case QueueType::compute:
        {
            cmd_buffer.queue = &device.compute_queue;
            break;
        }
        case QueueType::transfer:
        {
            cmd_buffer.queue = &device.transfer_queue;
            break;
        }
        default:
        {
            /*
             * use the graphics queue for both explicit graphics operations and any other combination
             * of queue type flags assuming a shared graphics/compute/transfer queue on most hardware
             * is available
             */
            cmd_buffer.queue = &device.graphics_queue;
            break;
        }
    }

    cmd_buffer.reset(&device);
    return &cmd_buffer;
}

void CommandBuffer::reset(VulkanDevice* new_device)
{
    state = CommandBufferState::initial;
    device = new_device;
    target_swapchain = -1;
    bound_pipeline = nullptr;
    memset(descriptors, VK_NULL_HANDLE, static_array_length(descriptors) * sizeof(VkDescriptorSet));
}

RenderPassHandle create_render_pass(const DeviceHandle& device_handle, const RenderPassCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    auto subpasses = FixedArray<VkSubpassDescription>::with_size(create_info.subpass_count, temp_allocator());
    auto attachments = FixedArray<VkAttachmentDescription>::with_size(create_info.attachment_count, temp_allocator());
    auto subpass_deps = FixedArray<VkSubpassDependency>::with_size(create_info.subpass_count, temp_allocator());

    auto vk_info = VkRenderPassCreateInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.attachmentCount = create_info.attachment_count;
    vk_info.pAttachments = attachments.data();
    vk_info.subpassCount = create_info.subpass_count;
    vk_info.pSubpasses = subpasses.data();
    vk_info.dependencyCount = 0;
    vk_info.pDependencies = nullptr;

    for (int a = 0; a < attachments.size(); ++a)
    {
        auto& attachment = attachments[a];
        const auto& bee_attachment = create_info.attachments[a];

        attachment.flags = 0;
        attachment.format = convert_pixel_format(bee_attachment.format);
        attachment.samples = decode_sample_count(bee_attachment.samples);
        attachment.loadOp = convert_load_op(bee_attachment.load_op);
        attachment.storeOp = convert_store_op(bee_attachment.store_op);
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        switch (bee_attachment.type)
        {
            case AttachmentType::color:
            {
                attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;
            }
            case AttachmentType::depth_stencil:
            {
                attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                break;
            }
            case AttachmentType::present:
            {
                attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    DynamicArray<VkAttachmentReference> attachment_refs(temp_allocator());

    for (int sp = 0; sp < subpasses.size(); ++sp)
    {
        auto& subpass = subpasses[sp];
        const auto& bee_subpass = create_info.subpasses[sp];

        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = bee_subpass.input_attachment_count;
        subpass.colorAttachmentCount = bee_subpass.color_attachment_count;
        subpass.preserveAttachmentCount = bee_subpass.preserve_attachment_count;
        subpass.pInputAttachments = nullptr;
        subpass.pColorAttachments = nullptr;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.pPreserveAttachments = nullptr;

        const auto this_subpass_begin = attachment_refs.size();

        // reserve a range of attachment refs for this subpass
        const auto this_subpass_count = bee_subpass.color_attachment_count
            + bee_subpass.input_attachment_count
            + bee_subpass.resolve_attachment_count
            + 1; // reserve one for the depth stencil if set

        attachment_refs.append(this_subpass_count, VkAttachmentReference{});

        auto* input_attachments = attachment_refs.data() + this_subpass_begin;
        auto* color_attachments = input_attachments + bee_subpass.input_attachment_count;
        auto* resolve_attachments = color_attachments + bee_subpass.color_attachment_count;
        auto* depth_stencil_attachment = resolve_attachments + bee_subpass.resolve_attachment_count;

        for (u32 att = 0; att < bee_subpass.input_attachment_count; ++att)
        {
            const auto index = bee_subpass.input_attachments[att];
            input_attachments[att].attachment = index;
            input_attachments[att].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        for (u32 att = 0; att < bee_subpass.color_attachment_count; ++att)
        {
            const auto index = bee_subpass.color_attachments[att];
            color_attachments[att].attachment = index;
            color_attachments[att].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        for (u32 att = 0; att < bee_subpass.resolve_attachment_count; ++att)
        {
            const auto index = bee_subpass.resolve_attachments[att];
            resolve_attachments[att].attachment = index;
            resolve_attachments[att].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if (bee_subpass.input_attachment_count > 0)
        {
            subpass.pInputAttachments = input_attachments;
        }
        if (bee_subpass.color_attachment_count > 0)
        {
            subpass.pColorAttachments = color_attachments;
        }
        if (bee_subpass.resolve_attachment_count > 0)
        {
            subpass.pResolveAttachments = resolve_attachments;
        }
        if (bee_subpass.depth_stencil < BEE_GPU_MAX_ATTACHMENTS)
        {
            subpass.pDepthStencilAttachment = depth_stencil_attachment;

            depth_stencil_attachment->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_stencil_attachment->attachment = create_info.subpasses[sp].depth_stencil;
            attachments[bee_subpass.depth_stencil].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments[bee_subpass.depth_stencil].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        if (bee_subpass.preserve_attachment_count > 0)
        {
            subpass.pPreserveAttachments = bee_subpass.preserve_attachments;
        }

        auto& dep = subpass_deps[sp];
        dep.dependencyFlags = 0;

        if (sp == 0)
        {
            // the first subpass has an external dependency
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.srcAccessMask = 0;

            if (bee_subpass.color_attachment_count > 0)
            {
                dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            else
            {
                dep.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
        }
        else
        {
            // figure out subpass->subpass src dependency
            dep.srcSubpass = static_cast<u32>(sp - 1);
            dep.srcStageMask = 0;
            dep.srcAccessMask = 0;

            const auto& prev_subpass = create_info.subpasses[sp - 1];

            if (prev_subpass.color_attachment_count > 0)
            {
                dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }

            if (prev_subpass.depth_stencil != BEE_GPU_MAX_ATTACHMENTS)
            {
                dep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
        }

        if (sp == subpasses.size() - 1 && subpasses.size() > 1)
        {
            // last subpass has external dep
            dep.dstSubpass = VK_SUBPASS_EXTERNAL;
            dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dep.dstAccessMask = 0;
        }
        else
        {
            // figure out subpass->subpass dst dependency
            dep.dstSubpass = static_cast<u32>(sp);
            dep.dstStageMask = 0;
            dep.dstAccessMask = 0;

            if (bee_subpass.input_attachment_count > 0)
            {
                dep.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dep.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            }

            if (bee_subpass.color_attachment_count > 0 || bee_subpass.resolve_attachment_count > 0)
            {
                dep.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            }

            if (bee_subpass.depth_stencil != BEE_GPU_MAX_ATTACHMENTS)
            {
                dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            }
        }
    }

    vk_info.dependencyCount = subpass_deps.size();
    vk_info.pDependencies = subpass_deps.data();

    auto* render_pass = BEE_NEW(thread.allocator, VulkanRenderPass);
    const RenderPassHandle handle { thread.render_passes.add(render_pass) };

    render_pass->create_info = create_info;

    BEE_VK_CHECK(vkCreateRenderPass(device.handle, &vk_info, nullptr, &render_pass->handle));

    return handle;
}

void destroy_render_pass(const DeviceHandle& device_handle, const RenderPassHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(handle);
    auto* render_pass = static_cast<VulkanRenderPass*>(thread.render_passes.remove(handle));
    vkDestroyRenderPass(device.handle, render_pass->handle, nullptr);
    thread.add_pending_delete(render_pass);
}

ShaderHandle create_shader(const DeviceHandle& device_handle, const ShaderCreateInfo& info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    VkShaderModuleCreateInfo vk_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.codeSize = info.code_size;
    vk_info.pCode = reinterpret_cast<const u32*>(info.code);

    auto* shader = BEE_NEW(thread.allocator, VulkanShader);
    const auto handle = thread.shaders.add(shader);
    shader->entry = info.entry;

    BEE_VK_CHECK(vkCreateShaderModule(device.handle, &vk_info, nullptr, &shader->handle));

    return ShaderHandle { handle };
}

void destroy_shader(const DeviceHandle& device_handle, const ShaderHandle& shader_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(shader_handle);
    auto* shader = static_cast<VulkanShader*>(thread.shaders.remove(shader_handle));
    vkDestroyShaderModule(device.handle, shader->handle, nullptr);
    thread.add_pending_delete(shader);
}

PipelineStateHandle create_pipeline_state(const DeviceHandle& device_handle, const PipelineStateCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);

    /*
     * Shader stages
     */
    struct StageInfo
    {
        ShaderHandle handle;
        VkShaderStageFlagBits flags { VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
    };

    StageInfo shaders[] = {
        { create_info.vertex_stage, VK_SHADER_STAGE_VERTEX_BIT },
        { create_info.fragment_stage, VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    DynamicArray<VkPipelineShaderStageCreateInfo> stages(temp_allocator());

    for (const auto& stage : shaders)
    {
        if (!stage.handle.is_valid())
        {
            continue;
        }

        auto& thread = device.get_thread(stage.handle);
        auto* shader = static_cast<VulkanShader*>(thread.shaders.get(stage.handle));

        stages.emplace_back();

        auto& stage_info = stages.back();
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext = nullptr;
        stage_info.flags = 0;
        stage_info.stage = stage.flags;
        stage_info.module = shader->handle;
        stage_info.pName = shader->entry.c_str();
        stage_info.pSpecializationInfo = nullptr;
    }

    /*
     * Vertex input state
     */
    auto vertex_binding_descs = FixedArray<VkVertexInputBindingDescription>::with_size(
        create_info.vertex_description.layout_count,
        temp_allocator()
    );
    auto vertex_attribute_descs = FixedArray<VkVertexInputAttributeDescription>::with_size(
        create_info.vertex_description.attribute_count,
        temp_allocator()
    );

    for (int b = 0; b < vertex_binding_descs.size(); ++b)
    {
        auto& vk_desc = vertex_binding_descs[b];
        const auto& layout = create_info.vertex_description.layouts[b];

        vk_desc.binding = layout.index;
        vk_desc.inputRate = convert_step_function(layout.step_function);
        vk_desc.stride = layout.stride;
    }

    for (int a = 0; a < vertex_attribute_descs.size(); ++a)
    {
        auto& vk_desc = vertex_attribute_descs[a];
        const auto& attr = create_info.vertex_description.attributes[a];

        vk_desc.location = attr.location;
        vk_desc.binding = attr.layout;
        vk_desc.format = convert_vertex_format(attr.format);
        vk_desc.offset = attr.offset;
    }

    VkPipelineVertexInputStateCreateInfo vertex_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
    vertex_info.flags = 0;
    vertex_info.vertexBindingDescriptionCount = static_cast<u32>(vertex_binding_descs.size());
    vertex_info.pVertexBindingDescriptions = vertex_binding_descs.data();
    vertex_info.vertexAttributeDescriptionCount = static_cast<u32>(vertex_attribute_descs.size());
    vertex_info.pVertexAttributeDescriptions = vertex_attribute_descs.data();

    /*
     * Input assembly state
     */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr };
    input_assembly_info.flags = 0;
    input_assembly_info.topology = convert_primitive_type(create_info.primitive_type);
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    /*
     * TODO(Jacob): Tessellation state
     */

    /*
     * Viewport state
     */
    // setup a default viewport state - is required by Vulkan but it's values aren't used if the pipeline uses dynamic states
    VkPipelineViewportStateCreateInfo default_viewport_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
    default_viewport_info.flags = 0;
    default_viewport_info.viewportCount = 1;
    default_viewport_info.scissorCount = 1;

    /*
     * Rasterization state
     */
    VkPipelineRasterizationStateCreateInfo raster_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr };
    raster_info.flags = 0;
    raster_info.depthClampEnable = static_cast<VkBool32>(create_info.raster_state.depth_clamp_enabled);
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = convert_fill_mode(create_info.raster_state.fill_mode);
    raster_info.cullMode = convert_cull_mode(create_info.raster_state.cull_mode);
    raster_info.frontFace = create_info.raster_state.front_face_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster_info.depthBiasEnable = static_cast<VkBool32>(create_info.raster_state.depth_bias_enabled);
    raster_info.depthBiasConstantFactor = create_info.raster_state.depth_bias;
    raster_info.depthBiasClamp = create_info.raster_state.depth_bias_clamp;
    raster_info.depthBiasSlopeFactor = create_info.raster_state.depth_slope_factor;
    raster_info.lineWidth = create_info.raster_state.line_width;

    /*
     * Multisample state
     */
    VkPipelineMultisampleStateCreateInfo multisample_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr };
    multisample_info.flags = 0;
    multisample_info.rasterizationSamples = static_cast<VkSampleCountFlagBits>(create_info.multisample_state.sample_count);
    multisample_info.sampleShadingEnable = static_cast<VkBool32>(create_info.multisample_state.sample_shading_enabled);
    multisample_info.minSampleShading = create_info.multisample_state.sample_shading;
    multisample_info.pSampleMask = &create_info.multisample_state.sample_mask;
    multisample_info.alphaToCoverageEnable = static_cast<VkBool32>(create_info.multisample_state.alpha_to_coverage_enabled);
    multisample_info.alphaToOneEnable = static_cast<VkBool32>(create_info.multisample_state.alpha_to_one_enabled);

    /*
     * Depth-stencil state
     */
    static auto convert_stencil_op_descriptor = [](const StencilOpDescriptor& from, VkStencilOpState* to)
    {
        to->failOp = convert_stencil_op(from.fail_op);
        to->passOp = convert_stencil_op(from.pass_op);
        to->depthFailOp = convert_stencil_op(from.depth_fail_op);
        to->compareOp = convert_compare_func(from.compare_func);
        to->compareMask = from.read_mask;
        to->writeMask = from.write_mask;
        to->reference = from.reference;
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
    depth_stencil_info.flags = 0;
    depth_stencil_info.depthTestEnable = static_cast<VkBool32>(create_info.depth_stencil_state.depth_test_enabled);
    depth_stencil_info.depthWriteEnable = static_cast<VkBool32>(create_info.depth_stencil_state.depth_write_enabled);
    depth_stencil_info.depthCompareOp = convert_compare_func(create_info.depth_stencil_state.depth_compare_func);
    depth_stencil_info.depthBoundsTestEnable = static_cast<VkBool32>(create_info.depth_stencil_state.depth_bounds_test_enabled);
    depth_stencil_info.stencilTestEnable = static_cast<VkBool32>(create_info.depth_stencil_state.stencil_test_enabled);
    convert_stencil_op_descriptor(create_info.depth_stencil_state.front_face_stencil, &depth_stencil_info.front);
    convert_stencil_op_descriptor(create_info.depth_stencil_state.back_face_stencil, &depth_stencil_info.front);
    depth_stencil_info.minDepthBounds = create_info.depth_stencil_state.min_depth_bounds;
    depth_stencil_info.maxDepthBounds = create_info.depth_stencil_state.max_depth_bounds;

    /*
     * Color blend state
     */
    auto color_blend_attachments = FixedArray<VkPipelineColorBlendAttachmentState>::with_size(
        create_info.color_blend_state_count,
        temp_allocator()
    );

    for (int i = 0; i < color_blend_attachments.size(); ++i)
    {
        auto& vk_state = color_blend_attachments[i];
        const auto& state = create_info.color_blend_states[i];

        vk_state.blendEnable = static_cast<VkBool32>(state.blend_enabled);
        vk_state.srcColorBlendFactor = convert_blend_factor(state.src_blend_color);
        vk_state.dstColorBlendFactor = convert_blend_factor(state.dst_blend_color);
        vk_state.colorBlendOp = convert_blend_op(state.color_blend_op);
        vk_state.srcAlphaBlendFactor = convert_blend_factor(state.src_blend_alpha);
        vk_state.dstAlphaBlendFactor = convert_blend_factor(state.dst_blend_alpha);
        vk_state.alphaBlendOp = convert_blend_op(state.alpha_blend_op);
        vk_state.colorWriteMask = decode_color_write_mask(state.color_write_mask);
    }

    VkPipelineColorBlendStateCreateInfo color_blend_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };
    color_blend_info.flags = 0;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_CLEAR;
    color_blend_info.attachmentCount = create_info.color_blend_state_count;
    color_blend_info.pAttachments = color_blend_attachments.data();
    color_blend_info.blendConstants[0] = 0.0f; // r
    color_blend_info.blendConstants[1] = 0.0f; // g
    color_blend_info.blendConstants[2] = 0.0f; // b
    color_blend_info.blendConstants[3] = 0.0f; // a

    /*
     * Dynamic state
     */
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr };
    dynamic_state_info.flags = 0;
    dynamic_state_info.dynamicStateCount = static_array_length(dynamic_states);
    dynamic_state_info.pDynamicStates = dynamic_states;

    /*
     * Pipeline layout
     */
    VulkanPipelineLayoutKey pipeline_layout_key{};
    pipeline_layout_key.resource_layout_count = create_info.resource_layout_count;
    pipeline_layout_key.resource_layouts = create_info.resource_layouts;
    pipeline_layout_key.push_constant_range_count = create_info.push_constant_range_count;
    pipeline_layout_key.push_constant_ranges = create_info.push_constant_ranges;
    auto& pipeline_layout = device.pipeline_layout_cache.get_or_create(pipeline_layout_key);

    /*
     * TODO(Jacob): Pipeline cache
     */


    /*
     * Setup the pipeline state info
     */
    VkGraphicsPipelineCreateInfo info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
    info.flags = 0;
    info.stageCount = static_cast<u32>(stages.size());
    info.pStages = stages.data();
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &input_assembly_info;
    info.pTessellationState = nullptr;
    info.pViewportState = &default_viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &multisample_info;
    info.pDepthStencilState = &depth_stencil_info;
    info.pColorBlendState = &color_blend_info;
    info.pDynamicState = &dynamic_state_info;
    info.layout = pipeline_layout;
    info.subpass = create_info.subpass_index;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;

    auto& rp_thread = device.get_thread(create_info.compatible_render_pass);
    info.renderPass = static_cast<VulkanRenderPass*>(rp_thread.render_passes.get(create_info.compatible_render_pass))->handle;

    // phew, that was a lot of typing - I think we earned ourselves a nice graphics pipeline object
    auto& thread = device.get_thread();
    auto* pipeline = BEE_NEW(thread.allocator, VulkanPipelineState);
    pipeline->layout = pipeline_layout;
    const auto handle = thread.pipeline_states.add(pipeline);

    BEE_VK_CHECK(vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline->handle));

    return PipelineStateHandle { handle };
}

void destroy_pipeline_state(const DeviceHandle& device_handle, const PipelineStateHandle& pipeline_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(pipeline_handle);
    auto* pipeline = static_cast<VulkanPipelineState*>(thread.pipeline_states.remove(pipeline_handle));
    vkDestroyPipeline(device.handle, pipeline->handle, nullptr);
    thread.add_pending_delete(pipeline);
}

BufferHandle create_buffer(const DeviceHandle& device_handle, const BufferCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);

    VkBufferCreateInfo vk_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.size = create_info.size;
    vk_info.usage = decode_buffer_type(create_info.type);
    vk_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO(Jacob): look into supporting concurrent queues
    vk_info.queueFamilyIndexCount = 0; // ignored if sharingMode != VK_SHARING_MODE_CONCURRENT
    vk_info.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo vma_info{};
    vma_info.flags = 0;
    vma_info.requiredFlags = 0;
    vma_info.preferredFlags = 0;
    vma_info.memoryTypeBits = 0;
    vma_info.pool = VK_NULL_HANDLE;
    vma_info.pUserData = nullptr; // TODO(Jacob): could be used to tag allocations?

    auto& thread = device.get_thread();
    auto* buffer = BEE_NEW(thread.allocator, VulkanBuffer)(create_info.type, create_info.memory_usage, create_info.size);
    const auto handle = thread.buffers.add(buffer);

    BEE_VK_CHECK(vmaCreateBuffer(
        device.vma_allocator,
        &vk_info,
        &vma_info,
        &buffer->handle,
        &buffer->allocation,
        &buffer->allocation_info
    ));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(&device, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, buffer->handle, create_info.debug_name);
    }

    return BufferHandle { handle };
}

void destroy_buffer(const DeviceHandle& device_handle, const BufferHandle& buffer_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(buffer_handle);
    auto* buffer = static_cast<VulkanBuffer*>(thread.buffers.remove(buffer_handle));
    BEE_ASSERT(buffer->handle != VK_NULL_HANDLE);
    BEE_ASSERT(buffer->allocation != VK_NULL_HANDLE);

    vmaDestroyBuffer(device.vma_allocator, buffer->handle, buffer->allocation);

    thread.add_pending_delete(buffer);
}

void update_buffer(const DeviceHandle& device_handle, const BufferHandle& buffer_handle, const void* data, const size_t offset, const size_t size)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto* buffer = device.buffers_get(buffer_handle);

    if (buffer->usage == DeviceMemoryUsage::gpu_only)
    {
        VulkanStagingChunk chunk{};
        thread.staging.allocate(size, 1, &chunk);

        memcpy(chunk.data, data, size);

        VkBufferCopy copy{};
        copy.srcOffset = chunk.offset;
        copy.dstOffset = offset;
        copy.size = size;

        vkCmdCopyBuffer(chunk.cmd, chunk.buffer, buffer->handle, 1, &copy);
    }
    else
    {
        void* mapped = nullptr;
        BEE_VK_CHECK(vmaMapMemory(device.vma_allocator, buffer->allocation, &mapped));
        memcpy(static_cast<u8*>(mapped) + offset, data, size);
        vmaUnmapMemory(device.vma_allocator, buffer->allocation);

//        // If the handle is host-coherent we need to flush the range manually
//        VkMemoryPropertyFlags mem_type_flags = 0;
//        vmaGetMemoryTypeProperties(device.vma_allocator, buffer.allocation_info.memoryType, &mem_type_flags);
//
//        if ((mem_type_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
//        {
//            VkMappedMemoryRange
//            auto memory_range = vk_make_struct<>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
//            memory_range.memory = buffer->allocation_info.deviceMemory;
//            memory_range.offset = buffer->allocation_info.offset;
//            memory_range.size = buffer->allocation_info.size;
//
//            SKY_VK_CHECK(vkFlushMappedMemoryRanges(device_, 1, &memory_range));
//        }
    }
}

TextureHandle create_texture(const DeviceHandle& device_handle, const TextureCreateInfo& create_info)
{
    BEE_ASSERT_F(create_info.width > 0 && create_info.height > 0, "Texture cannot be zero-sized");
    BEE_ASSERT_F(create_info.mip_count > 0, "Texture must have at least one mip level");
    BEE_ASSERT_F(create_info.array_element_count > 0, "Texture must have at least one array layer");

    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    auto image_info = VkImageCreateInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr };
    image_info.imageType = convert_image_type(create_info.type);
    image_info.format = convert_pixel_format(create_info.format);
    image_info.extent.width = create_info.width;
    image_info.extent.height = create_info.height;
    image_info.extent.depth = create_info.depth;
    image_info.mipLevels = create_info.mip_count;
    image_info.arrayLayers = create_info.array_element_count;
    image_info.samples = static_cast<VkSampleCountFlagBits>(create_info.sample_count);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = decode_image_usage(create_info.usage);
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO(Jacob): look into supporting concurrent queues
    image_info.queueFamilyIndexCount = 0; // ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT
    image_info.pQueueFamilyIndices = nullptr;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto* texture = BEE_NEW(thread.allocator, VulkanTexture);
    auto handle = thread.textures.add(texture);
    texture->width = create_info.width;
    texture->height = create_info.height;
    texture->layers = create_info.array_element_count;
    texture->levels = create_info.mip_count;
    texture->format = create_info.format;
    texture->samples = create_info.sample_count;
    texture->usage = create_info.usage;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = 0;
    alloc_info.usage = convert_memory_usage(create_info.memory_usage);
    alloc_info.requiredFlags = 0;
    alloc_info.preferredFlags = 0;
    alloc_info.memoryTypeBits = 0;
    alloc_info.pool = VK_NULL_HANDLE;
    alloc_info.pUserData = nullptr;

    BEE_VMA_CHECK(vmaCreateImage(
        device.vma_allocator,
        &image_info,
        &alloc_info,
        &texture->handle,
        &texture->allocation,
        &texture->allocation_info
    ));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(&device, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture->handle, create_info.debug_name);
    }

    return TextureHandle { handle };
}

void destroy_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(texture_handle);
    auto* texture = static_cast<VulkanTexture*>(thread.textures.get(texture_handle));

    // swapchain images are destroyed with their owning swapchain
    if (texture->swapchain < 0)
    {
        vmaDestroyImage(device.vma_allocator, texture->handle, texture->allocation);
        thread.textures.remove(texture_handle);
        thread.add_pending_delete(texture);
    }
}

void update_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle, const void* data, const Offset& offset, const Extent& extent, const u32 mip_level, const u32 element)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto* texture = device.textures_get(texture_handle);

    VulkanStagingChunk chunk{};
    thread.staging.allocate(texture->width * texture->height * texture->depth, 1, &chunk);

    const auto size = extent.width * extent.height * extent.depth;
    memcpy(chunk.data, data, size);

    VkBufferImageCopy copy{};
    copy.bufferOffset = chunk.offset;
    copy.imageSubresource.aspectMask = is_depth_format(texture->format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = mip_level;
    copy.imageSubresource.baseArrayLayer = element;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x = offset.x;
    copy.imageOffset.y = offset.y;
    copy.imageOffset.z = offset.z;
    copy.imageExtent.width = extent.width;
    copy.imageExtent.height = extent.height;
    copy.imageExtent.depth = extent.depth;

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture->handle;
    barrier.subresourceRange.aspectMask = select_access_mask_from_format(texture->format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = texture->levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = texture->layers;

    vkCmdPipelineBarrier(
        chunk.cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkCmdCopyBufferToImage(chunk.cmd, chunk.buffer, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

bool create_texture_view_internal(VulkanDevice* device, const TextureViewCreateInfo& create_info, VulkanTextureView* dst)
{
    auto* texture = device->textures_get(create_info.texture);

    VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
    view_info.flags = 0;
    view_info.image = texture->handle;
    view_info.viewType = convert_image_view_type(create_info.type);
    view_info.format = convert_pixel_format(create_info.format);
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = select_access_mask_from_format(create_info.format);
    view_info.subresourceRange.baseMipLevel = create_info.mip_level_offset;
    view_info.subresourceRange.levelCount = create_info.mip_level_count;
    view_info.subresourceRange.baseArrayLayer = create_info.array_element_offset;
    view_info.subresourceRange.layerCount = create_info.array_element_count;

    VkImageView img_view = VK_NULL_HANDLE;
    BEE_VK_CHECK(vkCreateImageView(device->handle, &view_info, nullptr, &img_view));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, img_view, create_info.debug_name);
    }

    dst->handle = img_view;
    dst->viewed_texture = create_info.texture;
    dst->format = texture->format;
    dst->samples = texture->samples;
    return true;
}

TextureViewHandle create_texture_view(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto* texture_view = BEE_NEW(thread.allocator, VulkanTextureView);

    if (!create_texture_view_internal(&device, create_info, texture_view))
    {
        BEE_DELETE(thread.allocator, texture_view);
        return TextureViewHandle{};
    };

    return TextureViewHandle { thread.texture_views.add(texture_view) };
}

void destroy_texture_view(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(texture_view_handle);
    auto* texture_view = static_cast<VulkanTextureView*>(thread.texture_views.remove(texture_view_handle));

    BEE_ASSERT(texture_view->handle != VK_NULL_HANDLE);

    vkDestroyImageView(device.handle, texture_view->handle, nullptr);

    thread.add_pending_delete(texture_view);
}

FenceHandle create_fence(const DeviceHandle& device_handle, const FenceState initial_state)
{
    auto& device = validate_device(device_handle);

    VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
    info.flags = initial_state == FenceState::signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

    VkFence fence = VK_NULL_HANDLE;
    BEE_VK_CHECK(vkCreateFence(device.handle, &info, nullptr, &fence));

    return FenceHandle { device.fences_add(fence) };
}

void destroy_fence(const DeviceHandle& device_handle, const FenceHandle& fence_handle)
{
    auto& device = validate_device(device_handle);
    auto* fence = static_cast<VkFence>(device.fences_remove(fence_handle));
    vkDestroyFence(device.handle, fence, nullptr);
}

void VulkanDescriptorPoolCache::clear_pending(VkDevice device)
{
    for (auto& pool : to_destroy_pools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    to_destroy_pools.clear();
}

void VulkanDescriptorPoolCache::destroy(VkDevice device)
{
    clear_pending(device);
    for (auto& descriptor_pool : pools)
    {
        vkDestroyDescriptorPool(device, descriptor_pool.value->handle, nullptr);
        BEE_DELETE(thread->allocator, descriptor_pool.value);
    }
    pools.clear();
    thread = nullptr;
}

void VulkanDescriptorPoolCache::reset(VkDevice device)
{
    for (auto& pool : pools)
    {
        BEE_VK_CHECK(vkResetDescriptorPool(device, pool.value->handle, 0));
        pool.value->allocated_sets = 0;
    }
}

VulkanDescriptorPool* get_or_create_descriptor_pool(VulkanDevice* device, const ResourceBindingUpdateFrequency update_frequency, const ResourceLayoutDescriptor& layout)
{
    static constexpr u32 growth_rate = 2;
    static constexpr u32 base_max_sets = 64;

    auto& thread = device->get_thread();
    VulkanDescriptorPoolCache* descriptor_pools = nullptr;

    switch (update_frequency)
    {
        case ResourceBindingUpdateFrequency::per_frame:
        case ResourceBindingUpdateFrequency::per_draw:
        {
            descriptor_pools = &thread.dynamic_descriptor_pools[device->current_frame];
            break;
        }
        case ResourceBindingUpdateFrequency::persistent:
        {
            descriptor_pools = &thread.static_descriptor_pools;
            break;
        }
    }

    auto* pool_keyval = descriptor_pools->pools.find(layout);

    if (pool_keyval == nullptr)
    {
        // couldn't find a matching pool so we need to create a new cached one
        pool_keyval = descriptor_pools->pools.insert(layout, BEE_NEW(thread.allocator, VulkanDescriptorPool));

        pool_keyval->value->size_count = layout.resource_count;
        pool_keyval->value->layout = device->descriptor_set_layout_cache.get_or_create(layout);

        // initialize the pool sizes
        for (u32 i = 0; i < layout.resource_count; ++i)
        {
            pool_keyval->value->sizes[i].type = convert_resource_binding_type(layout.resources[i].type);
            pool_keyval->value->sizes[i].descriptorCount = 0u;
        }
    }

    auto* pool = pool_keyval->value;

    if (pool->allocated_sets >= pool->max_sets || pool->max_sets == 0)
    {
        if (pool->handle != VK_NULL_HANDLE)
        {
            descriptor_pools->to_destroy_pools.push_back(pool->handle);
        }

        pool->thread = &thread;
        pool->handle = VK_NULL_HANDLE;
        pool->max_sets = math::max(pool->allocated_sets * growth_rate, base_max_sets);

        for (u32 i = 0; i < pool->size_count; ++i)
        {
            pool->sizes[i].descriptorCount = math::max(pool->sizes[i].descriptorCount * growth_rate, 1u);
        }

        VkDescriptorPoolCreateInfo info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
        info.flags = 0;
        info.maxSets = pool->max_sets;
        info.poolSizeCount = pool->size_count;
        info.pPoolSizes = pool->sizes;

        if (update_frequency == ResourceBindingUpdateFrequency::persistent)
        {
            info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        }

        BEE_VK_CHECK(vkCreateDescriptorPool(device->handle, &info, nullptr, &pool->handle));
    }

    return pool;
}

ResourceBindingHandle create_resource_binding(const DeviceHandle& device_handle, const ResourceBindingCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    auto* pool = get_or_create_descriptor_pool(&device, create_info.update_frequency, *create_info.layout);
    auto* binding = BEE_NEW(thread.allocator, VulkanResourceBinding);
    binding->allocated_frame = device.current_frame;
    binding->next = nullptr;
    binding->update_frequency = create_info.update_frequency;
    binding->layout = *create_info.layout;
    binding->set = VK_NULL_HANDLE;
    binding->pool = pool;

    // bindings with frame/draw update frequencies are allocated when binding in the command buffer
    // so we only need to allocate a descriptor up front for persistent bindings
    if (create_info.update_frequency == ResourceBindingUpdateFrequency::persistent)
    {
        VkDescriptorSetAllocateInfo set_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
        set_info.descriptorPool = pool->handle;
        set_info.descriptorSetCount = 1;
        set_info.pSetLayouts = &pool->layout;

        BEE_VK_CHECK(vkAllocateDescriptorSets(device.handle, &set_info, &binding->set));
    }

    return ResourceBindingHandle { thread.resource_bindings.add(binding) };
}

void destroy_resource_binding(const DeviceHandle& device_handle, const ResourceBindingHandle& resource_binding_handle)
{
    auto& device = validate_device(device_handle);
    auto* binding = device.resource_bindings_remove(resource_binding_handle);

    if (binding->update_frequency == ResourceBindingUpdateFrequency::persistent)
    {
        auto& thread = device.get_thread();
        if (thread.static_resource_binding_pending_deletes == nullptr)
        {
            thread.static_resource_binding_pending_deletes = binding;
        }
        else
        {
            thread.static_resource_binding_pending_deletes->next = binding;
        }

        binding->next = nullptr;
    }
    else
    {
        auto& thread = device.get_thread(resource_binding_handle);
        thread.add_pending_delete(binding);
    }
}

SamplerHandle create_sampler(const DeviceHandle& device_handle, const SamplerCreateInfo& info)
{
    auto& device = validate_device(device_handle);

    VkSamplerCreateInfo vkinfo      = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr };
    vkinfo.flags                    = 0;
    vkinfo.magFilter                = convert_filter(info.mag_filter);
    vkinfo.minFilter                = convert_filter(info.min_filter);
    vkinfo.mipmapMode               = convert_mip_map_mode(info.mip_mode);
    vkinfo.addressModeU             = convert_address_mode(info.u_address);
    vkinfo.addressModeV             = convert_address_mode(info.v_address);
    vkinfo.addressModeW             = convert_address_mode(info.w_address);
    vkinfo.mipLodBias               = info.lod_bias;
    vkinfo.anisotropyEnable         = static_cast<VkBool32>(info.anisotropy_enabled);
    vkinfo.maxAnisotropy            = info.anisotropy_max;
    vkinfo.compareEnable            = static_cast<VkBool32>(info.compare_enabled);
    vkinfo.compareOp                = convert_compare_func(info.compare_func);
    vkinfo.minLod                   = info.lod_min;
    vkinfo.maxLod                   = info.lod_max;
    vkinfo.borderColor              = convert_border_color(info.border_color);
    vkinfo.unnormalizedCoordinates  = static_cast<VkBool32>(!info.normalized_coordinates);

    VkSampler sampler = VK_NULL_HANDLE;
    BEE_VK_CHECK(vkCreateSampler(device.handle, &vkinfo, nullptr, &sampler));

    return SamplerHandle { device.samplers_add(sampler) };
};

void destroy_sampler(const DeviceHandle& device_handle, const SamplerHandle& sampler_handle)
{
    auto& device = validate_device(device_handle);
    auto* sampler = device.samplers_remove(sampler_handle);
    vkDestroySampler(device.handle, sampler, nullptr);
}


} // namespace bee


static bee::GpuModule g_gpu;

void bee_load_cmd_module(bee::PluginRegistry* registry, const bee::PluginState state);

BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_backend = registry->get_or_create_persistent<bee::VulkanBackend>("BeeVulkanBackend");

    g_gpu.init = bee::init;
    g_gpu.destroy = bee::destroy;
    g_gpu.enumerate_physical_devices = bee::enumerate_physical_devices;
    g_gpu.create_device = bee::create_device;
    g_gpu.destroy_device = bee::destroy_device;
    g_gpu.device_wait = bee::device_wait;
    g_gpu.create_swapchain = bee::create_swapchain;
    g_gpu.destroy_swapchain = bee::destroy_swapchain;
    g_gpu.acquire_swapchain_texture = bee::acquire_swapchain_texture;
    g_gpu.get_swapchain_texture_view = bee::get_swapchain_texture_view;
    g_gpu.get_swapchain_extent = bee::get_swapchain_extent;
    g_gpu.get_swapchain_texture_format = bee::get_swapchain_texture_format;
    g_gpu.get_texture_format = bee::get_texture_format;
    g_gpu.submit = bee::submit;
    g_gpu.present = bee::present;
    g_gpu.commit_frame = bee::commit_frame;
    g_gpu.get_current_frame = bee::get_current_frame;

    // Resource functions
    g_gpu.allocate_command_buffer = bee::allocate_command_buffer;
    g_gpu.create_render_pass = bee::create_render_pass;
    g_gpu.destroy_render_pass = bee::destroy_render_pass;
    g_gpu.create_shader = bee::create_shader;
    g_gpu.destroy_shader = bee::destroy_shader;
    g_gpu.create_pipeline_state = bee::create_pipeline_state;
    g_gpu.destroy_pipeline_state = bee::destroy_pipeline_state;
    g_gpu.create_buffer = bee::create_buffer;
    g_gpu.destroy_buffer = bee::destroy_buffer;
    g_gpu.update_buffer = bee::update_buffer;
    g_gpu.create_texture = bee::create_texture;
    g_gpu.destroy_texture = bee::destroy_texture;
    g_gpu.update_texture = bee::update_texture;
    g_gpu.create_texture_view = bee::create_texture_view;
    g_gpu.destroy_texture_view = bee::destroy_texture_view;
    g_gpu.create_fence = bee::create_fence;
    g_gpu.destroy_fence = bee::destroy_fence;
    g_gpu.create_resource_binding = bee::create_resource_binding;
    g_gpu.destroy_resource_binding = bee::destroy_resource_binding;
    g_gpu.create_sampler = bee::create_sampler;
    g_gpu.destroy_sampler = bee::destroy_sampler;

    registry->toggle_module(state, BEE_GPU_MODULE_NAME, &g_gpu);

    bee_load_cmd_module(registry, state);
}