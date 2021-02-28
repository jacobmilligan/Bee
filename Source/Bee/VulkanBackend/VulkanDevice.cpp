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
#include "Bee/Core/Bit.hpp"
#include "Bee/Core/Sort.hpp"

#include "Bee/Gpu/Gpu.hpp"

#include "Bee/VulkanBackend/VulkanDevice.hpp"
#include "Bee/VulkanBackend/VulkanConvert.hpp"

namespace bee {


static PlatformModule* g_platform = nullptr;

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

GpuApi get_api()
{
    return GpuApi::vulkan;
}

const char* get_name()
{
    return "Bee.VulkanBackend";
}

bool is_initialized()
{
    return g_backend->instance != nullptr;
}

GpuCommandBackend* get_command_backend()
{
    return &g_backend->command_backend;
}


#define BEE_GPU_VALIDATE_BACKEND() BEE_ASSERT_F(is_initialized(), "GPU backend has not been initialized")


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
CommandBuffer* allocate_command_buffer(const DeviceHandle& device_handle, const QueueType queue);

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

    new (&device) VulkanDevice{};

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

    if (supported_features.independentBlend == VK_TRUE)
    {
        enabled_features.independentBlend = VK_TRUE;
        log_info("VulkanBackend: Enabling device feature independentBlend");
    }

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
    vma_info.instance = g_backend->instance;

    BEE_VK_CHECK(vmaCreateAllocator(&vma_info, &device.vma_allocator));

    // initialize caches
    device.descriptor_set_layout_cache.init(&device, create_descriptor_set_layout, destroy_descriptor_set_layout);
    device.pipeline_layout_cache.init(&device, create_pipeline_layout, destroy_pipeline_layout);
    device.framebuffer_cache.init(&device, create_framebuffer, destroy_framebuffer);
    device.pipeline_cache.init(&device, create_pipeline, destroy_pipeline);

    // initialize thread-local data
    device.thread_data.resize(job_system_worker_count());
    for (u32 i = 0; i < static_cast<u32>(device.thread_data.size()); ++i)
    {
        new (&device.thread_data[i]) VulkanThreadData(i);
    }

    auto cmd_pool_info = VkCommandPoolCreateInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    cmd_pool_info.flags = 0;
    cmd_pool_info.queueFamilyIndex = device.graphics_queue.index;

    for (int i = 0; i < device.thread_data.size(); ++i)
    {
        auto& thread = device.thread_data[i];
        thread.index = i;

        // Initialize the staging buffers
        thread.staging.init(&device, device.vma_allocator);

        // Create command pool per thread per frame
        for (int frame = 0; frame < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            BEE_VK_CHECK(vkCreateCommandPool(device.handle, &cmd_pool_info, nullptr, &thread.command_pool[frame].handle));
        }
    }

    // Setup queue submissions
    for (int queue = 0; queue < vk_max_queues; ++queue)
    {
        device.submissions[queue].queue = queue;
    }

    return DeviceHandle(sign_cast<u32>(device_idx));
}

static void cleanup_command_buffers(VulkanDevice* device, VulkanCommandPool* pool)
{
    BEE_VK_CHECK(vkResetCommandPool(device->handle, pool->handle, 0));

    for (auto& cmd : pool->command_buffers)
    {
        if (cmd.handle != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(device->handle, pool->handle, 1, &cmd.handle);
        }
    }
}

static void submissions_wait(VulkanDevice* device, const i32 frame)
{
    scoped_recursive_lock_t lock(device->fence_mutex);

    if (device->used_submit_fences[frame].empty())
    {
        return;
    }

    // Wait on all the executing submissions from the new frame
    const auto wait_result = vkWaitForFences(
        device->handle,
        device->used_submit_fences[frame].size(),
        device->used_submit_fences[frame].data(),
        VK_TRUE,
        limits::max<u64>()
    );
    BEE_ASSERT_F(wait_result == VK_SUCCESS || wait_result == VK_TIMEOUT, "Vulkan: %s", vk_result_string(wait_result));

    BEE_VK_CHECK(vkResetFences(
        device->handle,
        device->used_submit_fences[frame].size(),
        device->used_submit_fences[frame].data()
    ));

    // Return the submit fences to the free pool
    for (auto& fence : device->used_submit_fences[frame])
    {
        device->free_submit_fences[frame].push_back(fence);
    }
    device->used_submit_fences[frame].clear();
}

void destroy_device(const DeviceHandle& device_handle)
{
    auto& device = validate_device(device_handle);

    for (int i = 0; i < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        submissions_wait(&device, i);
    }

    // Destroy all the fences in the free pool
    for (auto& fences : device.free_submit_fences)
    {
        for (auto& fence : fences)
        {
            if (fence != VK_NULL_HANDLE)
            {
                vkDestroyFence(device.handle, fence, nullptr);
            }
        }

        fences.clear();
    }

    // Destroy cached objects
    device.descriptor_set_layout_cache.destroy();
    device.pipeline_layout_cache.destroy();
    device.framebuffer_cache.destroy();
    device.pipeline_cache.destroy();

    // Destroy the vulkan-related thread data
    for (auto& thread : device.thread_data)
    {
        // Destroy any buffers that were dynamically sized
        for (auto& dynamic_buffer_deletes : thread.dynamic_buffer_deletes)
        {
            for (auto& buffer : dynamic_buffer_deletes)
            {
                vmaDestroyBuffer(device.vma_allocator, buffer.handle, buffer.allocation);
            }

            dynamic_buffer_deletes.clear();
        }

        for (auto& command_pool : thread.command_pool)
        {
            cleanup_command_buffers(&device, &command_pool);
            vkDestroyCommandPool(device.handle, command_pool.handle, nullptr);
        }

        thread.staging.destroy();

        for (auto& descriptor_cache : thread.dynamic_descriptor_pools)
        {
            descriptor_cache.destroy(device.handle);
        }

        thread.static_descriptor_pools.destroy(device.handle);
    }

    vmaDestroyAllocator(device.vma_allocator);
    vkDestroyDevice(device.handle, nullptr);
    destruct(&device);

    device.physical_device = VK_NULL_HANDLE;
    device.handle = VK_NULL_HANDLE;
    device.vma_allocator = VK_NULL_HANDLE;
}

void device_wait(const DeviceHandle& device_handle)
{
    vkDeviceWaitIdle(validate_device(device_handle).handle);
}

void submissions_wait(const DeviceHandle& device_handle)
{
    auto& device = validate_device(device_handle);

    for (int i = 0; i < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        submissions_wait(&device, i);
    }
}

extern void end(CommandBuffer* cmd_buf);
extern void begin(CommandBuffer* cmd_buf, const CommandBufferUsage usage);


CommandBuffer* VulkanThreadData::get_device_cmd(const DeviceHandle device_handle)
{
    auto& device = validate_device(device_handle);
    if (device_cmd[device.current_frame] == nullptr)
    {
        device_cmd[device.current_frame] = allocate_command_buffer(device_handle, QueueType::graphics);
        begin(device_cmd[device.current_frame], CommandBufferUsage::submit_once);
    }
    return device_cmd[device.current_frame];
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

void VulkanQueueSubmit::submit(VulkanDevice* device)
{
    if (cmd_buffers.empty())
    {
        return;
    }

    VkFence submit_fence = VK_NULL_HANDLE;
    {
        scoped_recursive_lock_t lock(device->fence_mutex);
        if (device->free_submit_fences[device->current_frame].empty())
        {
            VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
            fence_info.flags = 0;
            BEE_VK_CHECK(vkCreateFence(device->handle, &fence_info, nullptr, &submit_fence));
        }
        else
        {
            submit_fence = device->free_submit_fences[device->current_frame].back();
            device->free_submit_fences[device->current_frame].pop_back();
        }

        device->used_submit_fences[device->current_frame].push_back(submit_fence);
    }

    info.commandBufferCount = sign_cast<u32>(cmd_buffers.size());
    info.pCommandBuffers = cmd_buffers.data();
    device->queues[queue].submit(info, submit_fence, device);
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
        surface = vk_create_wsi_surface(g_backend->instance, g_platform->get_os_window(create_info.window));
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
        for (int i = 0; i < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++i)
        {
            submissions_wait(device, i);
        }

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

    swapchain->handle           = vk_handle;
    swapchain->surface          = surface;
    swapchain->selected_format  = convert_vk_format(selected_format.format);
    swapchain->create_info      = create_info;
    swapchain->create_info.texture_extent = actual_extent; // fixup the extent in the stored create info

    if (swapchain_info.oldSwapchain == VK_NULL_HANDLE)
    {
        swapchain->images       = FixedArray<TextureHandle>::with_size(image_count);
        swapchain->image_views  = FixedArray<TextureViewHandle>::with_size(image_count);
    }

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
    view_info.format                = swapchain->selected_format;
    view_info.mip_level_count       = 1;
    view_info.mip_level_offset      = 0;
    view_info.array_element_offset  = 0;
    view_info.array_element_count   = 1;

    auto& thread = device->get_thread();

    for (int si = 0; si < swapchain_images.size(); ++si)
    {
        if (!swapchain->images[si].is_valid())
        {
            swapchain->images[si] = thread.textures.allocate();
            auto& texture = thread.textures[swapchain->images[si]];

            texture.swapchain                       = swapchain_index;
            texture.create_info.width               = swapchain_info.imageExtent.width;
            texture.create_info.height              = swapchain_info.imageExtent.height;
            texture.create_info.array_element_count = swapchain_info.imageArrayLayers;
            texture.create_info.mip_count           = 1;
            texture.create_info.sample_count        = VK_SAMPLE_COUNT_1_BIT;
            texture.create_info.format              = swapchain->selected_format;
            texture.handle                          = swapchain_images[si];
            set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture.handle, "Swapchain image");
        }
        else
        {
            // TODO(Jacob): do we only need to reassign the texture params when recreating a swapchain?
            auto& texture = device->textures_get(swapchain->images[si]);
            texture.create_info.width               = swapchain_info.imageExtent.width;
            texture.create_info.height              = swapchain_info.imageExtent.height;
            texture.create_info.array_element_count = swapchain_info.imageArrayLayers;
            texture.create_info.format              = swapchain->selected_format;
            texture.handle                          = swapchain_images[si];
        }

        // Create a texture view as well
        view_info.texture               = swapchain->images[si];
        view_info.debug_name            = "Swapchain texture view";

        if (!swapchain->image_views[si].is_valid())
        {
            swapchain->image_views[si]      = thread.texture_views.allocate();
            auto& texture_view              = thread.texture_views[swapchain->image_views[si]];
            texture_view.swapchain         = swapchain_index;
            create_texture_view_internal(device, view_info, &texture_view);
        }
        else
        {
            // Recreate the image view if the swapchain is existing
            auto& texture_view = device->texture_views_get(swapchain->image_views[si]);
            vkDestroyImageView(device->handle, texture_view.handle, nullptr);
            create_texture_view_internal(device, view_info, &texture_view);
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

    for (int i = 0; i < swapchain.images.size(); ++i)
    {
        if (swapchain.image_views[i].is_valid())
        {
            auto handle = swapchain.image_views[i];
            auto& thread = device.get_thread(handle);
            auto& texture_view = thread.texture_views.deallocate(handle);
            vkDestroyImageView(device.handle, texture_view.handle, nullptr);
        }

        if (swapchain.images[i].is_valid())
        {
            auto& thread = device.get_thread(swapchain.images[i]);
            thread.textures.deallocate(swapchain.images[i]);
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
    return swapchain.selected_format;
}

PixelFormat get_texture_format(const DeviceHandle& device_handle, const TextureHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(handle);
    auto& texture = thread.textures[handle];
    return texture.create_info.format;
}

static void submit_device_commands(VulkanDevice* device)
{
    for (auto& thread : device->thread_data)
    {
        auto cmd = thread.device_cmd[device->current_frame];
        if (cmd == nullptr)
        {
            continue;
        }

        end(cmd);

        auto& submission = device->submissions[cmd->queue->index];
        submission.add(cmd);

        // reset to default state
        thread.device_cmd[device->current_frame] = nullptr;
    }
}

void submit(const DeviceHandle& device_handle, const SubmitInfo& info)
{
    BEE_ASSERT_MAIN_THREAD();

    static constexpr VkPipelineStageFlags swapchain_wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (info.command_buffer_count == 0)
    {
        log_warning("GPU warning: created a submit request with 0 command buffers");
        return;
    }

    BEE_ASSERT_F(info.command_buffers != nullptr, "`command_buffers` must point to an array of `command_buffer_count` GpuCommandBuffer pointers");

    auto& device = validate_device(device_handle);

    for (auto& thread : device.thread_data)
    {
        if (thread.staging.is_pending())
        {
            thread.staging.submit();
        }
    }

    // Reset the devices submission states for creating new ones
    for (auto& submit : device.submissions)
    {
        submit.reset();
    }

    submit_device_commands(&device);

    // Gather all the command buffers into per-queue submissions
    for (u32 i = 0; i < info.command_buffer_count; ++i)
    {
        auto* cmd = info.command_buffers[i];
        auto& submission = device.submissions[cmd->queue->index];

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

    for (auto& submission : device.submissions)
    {
        submission.submit(&device);
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
    if (BEE_FAIL_F(!swapchain.pending_image_acquire, "it is not valid to present a swapchain before acquiring its next texture index"))
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
    BEE_ASSERT_MAIN_THREAD();

    auto& device = validate_device(device_handle);

    scoped_recursive_lock_t lock(device.device_mutex);

    // submit any remaining device commands
    for (auto& submission : device.submissions)
    {
        submission.reset();
    }

    submit_device_commands(&device);

    for (auto& submission : device.submissions)
    {
        submission.submit(&device);
    }

    /*
     * We can't call vkFreeDescriptor sets without exclusive access to the pool so rather than
     * locking the pool each time we need to free a descriptor set we do it with the global device mutex
     * locked here in commit_frame (see: 3.6. Threading Behavior).
     */

    // process all the pending deletes now that we have exclusive access
    for (auto& thread : device.thread_data)
    {
        // free and then delete the pending static descriptor sets
        VulkanResourceBinding* binding_node = thread.static_resource_binding_pending_deletes;
        while (binding_node != nullptr)
        {
            auto* next = binding_node->next;
            --binding_node->pool->allocated_sets;
            vkFreeDescriptorSets(device.handle, binding_node->pool->handle, 1, &binding_node->set);
            binding_node = next;
        }

        thread.static_resource_binding_pending_deletes = nullptr;
    }

    device.descriptor_set_layout_cache.sync();
    device.pipeline_layout_cache.sync();
    device.framebuffer_cache.sync();
    device.current_frame = (device.current_frame + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;

    submissions_wait(&device, device.current_frame);

    // Reset all the per-thread command pools for the current frame
    for (auto& thread : device.thread_data)
    {
        // Handle all the deferred removals
        thread.flush_deallocations();

        auto& command_pool = thread.command_pool[device.current_frame];

        // Reset the threads command pool and start again with 0 in-use command buffers
        BEE_VK_CHECK(vkResetCommandPool(device.handle, command_pool.handle, 0));
        thread.command_pool[device.current_frame].command_buffer_count = 0;

        // Destroy pending descriptor pool deletes leftover from resizes
        thread.dynamic_descriptor_pools[device.current_frame].clear_pending(device.handle);
        thread.dynamic_descriptor_pools[device.current_frame].reset(device.handle);

        // Destroy any buffers that were dynamically sized
        for (auto& buffer : thread.dynamic_buffer_deletes[device.current_frame])
        {
            vmaDestroyBuffer(device.vma_allocator, buffer.handle, buffer.allocation);
        }

        thread.dynamic_buffer_deletes[device.current_frame].clear();
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
    viewport_dirty = false;
    scissor_dirty = false;
    memset(descriptors, VK_NULL_HANDLE, static_array_length(descriptors) * sizeof(VkDescriptorSet));
    memset(push_constants, 0, static_array_length(push_constants) * sizeof(const void*));
    memset(&viewport, 0, sizeof(Viewport));
    memset(&scissor, 0, sizeof(RenderRect));
}

RenderPassHandle create_render_pass(const DeviceHandle& device_handle, const RenderPassCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    auto subpasses = FixedArray<VkSubpassDescription>::with_size(create_info.subpass_count, temp_allocator());
    auto attachments = FixedArray<VkAttachmentDescription>::with_size(create_info.attachments.size, temp_allocator());
    auto subpass_deps = FixedArray<VkSubpassDependency>::with_size(create_info.subpass_count, temp_allocator());

    auto vk_info = VkRenderPassCreateInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.attachmentCount = create_info.attachments.size;
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
                attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;
            }
            case AttachmentType::depth_stencil:
            {
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
        subpass.inputAttachmentCount = bee_subpass.input_attachments.size;
        subpass.colorAttachmentCount = bee_subpass.color_attachments.size;
        subpass.preserveAttachmentCount = bee_subpass.preserve_attachments.size;
        subpass.pInputAttachments = nullptr;
        subpass.pColorAttachments = nullptr;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.pPreserveAttachments = nullptr;

        const auto this_subpass_begin = attachment_refs.size();

        // reserve a range of attachment refs for this subpass
        const auto this_subpass_count = bee_subpass.color_attachments.size
            + bee_subpass.input_attachments.size
            + bee_subpass.resolve_attachments.size
            + 1; // reserve one for the depth stencil if set

        attachment_refs.append(this_subpass_count, VkAttachmentReference{});

        auto* input_attachments = attachment_refs.data() + this_subpass_begin;
        auto* color_attachments = input_attachments + bee_subpass.input_attachments.size;
        auto* resolve_attachments = color_attachments + bee_subpass.color_attachments.size;
        auto* depth_stencil_attachment = resolve_attachments + bee_subpass.resolve_attachments.size;

        for (u32 att = 0; att < bee_subpass.input_attachments.size; ++att)
        {
            const auto index = bee_subpass.input_attachments[att];
            input_attachments[att].attachment = index;
            input_attachments[att].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        for (u32 att = 0; att < bee_subpass.color_attachments.size; ++att)
        {
            const auto index = bee_subpass.color_attachments[att];
            color_attachments[att].attachment = index;
            color_attachments[att].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        for (u32 att = 0; att < bee_subpass.resolve_attachments.size; ++att)
        {
            const auto index = bee_subpass.resolve_attachments[att];
            resolve_attachments[att].attachment = index;
            resolve_attachments[att].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if (bee_subpass.input_attachments.size > 0)
        {
            subpass.pInputAttachments = input_attachments;
        }
        if (bee_subpass.color_attachments.size > 0)
        {
            subpass.pColorAttachments = color_attachments;
        }
        if (bee_subpass.resolve_attachments.size > 0)
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
        if (bee_subpass.preserve_attachments.size > 0)
        {
            subpass.pPreserveAttachments = bee_subpass.preserve_attachments.data;
        }

        auto& dep = subpass_deps[sp];
        dep.dependencyFlags = 0;

        if (sp == 0)
        {
            // the first subpass has an external dependency
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.srcAccessMask = 0;

            if (bee_subpass.color_attachments.size > 0)
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

            if (prev_subpass.color_attachments.size > 0)
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

            if (bee_subpass.input_attachments.size > 0)
            {
                dep.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dep.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            }

            if (bee_subpass.color_attachments.size > 0 || bee_subpass.resolve_attachments.size > 0)
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

    const auto handle = thread.render_passes.allocate();
    auto& render_pass = thread.render_passes[handle];

    render_pass.create_info = create_info;
    render_pass.hash = get_hash(create_info);

    BEE_VK_CHECK(vkCreateRenderPass(device.handle, &vk_info, nullptr, &render_pass.handle));

    return handle;
}

void destroy_render_pass(const DeviceHandle& device_handle, const RenderPassHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(handle);
    auto& render_pass = thread.render_passes.deallocate(handle);
    vkDestroyRenderPass(device.handle, render_pass.handle, nullptr);
}

ShaderHandle create_shader(const DeviceHandle& device_handle, const ShaderCreateInfo& info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    VkShaderModuleCreateInfo vk_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.codeSize = info.code_size;
    vk_info.pCode = reinterpret_cast<const u32*>(info.code);

    const auto handle = thread.shaders.allocate();
    auto& shader = thread.shaders[handle];
    shader.entry = info.entry;
    shader.hash = get_hash(info.code, info.code_size, 0x123fd9);

    BEE_VK_CHECK(vkCreateShaderModule(device.handle, &vk_info, nullptr, &shader.handle));

    return handle;
}

void destroy_shader(const DeviceHandle& device_handle, const ShaderHandle& shader_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(shader_handle);
    auto& shader = thread.shaders.deallocate(shader_handle);
    vkDestroyShaderModule(device.handle, shader.handle, nullptr);
}

static void ensure_buffer_size(CommandBuffer* cmd_buf, VulkanBuffer* buffer)
{
    auto* device = cmd_buf->device;
    auto& thread = device->get_thread();

    // no need to resize the buffer if its size hasn't changed
    if (buffer->size <= buffer->allocation_info.size && buffer->handle != VK_NULL_HANDLE)
    {
        return;
    }

    // Destroy the old buffer in this frame if one exists
    if (buffer->handle != VK_NULL_HANDLE)
    {
        thread.dynamic_buffer_deletes[device->current_frame].emplace_back(buffer->handle, buffer->allocation);
    }

    VkBufferCreateInfo vk_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.size = buffer->size;
    vk_info.usage = decode_buffer_type(buffer->type);
    vk_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO(Jacob): look into supporting concurrent queues
    vk_info.queueFamilyIndexCount = 0; // ignored if sharingMode != VK_SHARING_MODE_CONCURRENT
    vk_info.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo vma_info{};
    vma_info.flags = 0;
    vma_info.usage = convert_memory_usage(buffer->usage);
    vma_info.requiredFlags = 0;
    vma_info.preferredFlags = 0;
    vma_info.memoryTypeBits = 0;
    vma_info.pool = VK_NULL_HANDLE;
    vma_info.pUserData = nullptr; // TODO(Jacob): could be used to tag allocations?

    VulkanBufferAllocation new_buffer;
    BEE_VK_CHECK(vmaCreateBuffer(
        device->vma_allocator,
        &vk_info,
        &vma_info,
        &new_buffer.handle,
        &new_buffer.allocation,
        &buffer->allocation_info
    ));

    if (buffer->debug_name != nullptr)
    {
        set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, new_buffer.handle, buffer->debug_name);
    }

    buffer->handle = new_buffer.handle;
    buffer->allocation = new_buffer.allocation;
}

BufferHandle create_buffer(const DeviceHandle& device_handle, const BufferCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    const auto handle = thread.buffers.allocate(create_info.type, create_info.memory_usage, create_info.size);
    auto& buffer = thread.buffers[handle];
    buffer.debug_name = create_info.debug_name;

    ensure_buffer_size(thread.get_device_cmd(device_handle), &buffer);

    return handle;
}

void destroy_buffer(const DeviceHandle& device_handle, const BufferHandle& buffer_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(buffer_handle);
    auto& buffer = thread.buffers.deallocate(buffer_handle);

    BEE_ASSERT(buffer.handle != VK_NULL_HANDLE);
    BEE_ASSERT(buffer.allocation != VK_NULL_HANDLE);

    vmaDestroyBuffer(device.vma_allocator, buffer.handle, buffer.allocation);
}

void update_buffer(const DeviceHandle& device_handle, const BufferHandle& buffer_handle, const void* data, const size_t offset, const size_t size)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto& buffer = device.buffers_get(buffer_handle);

    if (offset + size > buffer.size && BEE_CHECK_F(buffer.is_dynamic(), "Cannot grow buffer: not created with flag BufferType::dynamic_buffer"))
    {
        buffer.size = offset + size;
        ensure_buffer_size(thread.get_device_cmd(device_handle), &buffer);
    }

    if (buffer.usage == DeviceMemoryUsage::gpu_only)
    {
        VulkanStagingChunk chunk{};
        thread.staging.allocate(size, 1, &chunk);

        memcpy(chunk.data, data, size);

        VkBufferCopy copy{};
        copy.srcOffset = chunk.offset;
        copy.dstOffset = offset;
        copy.size = size;

        vkCmdCopyBuffer(chunk.cmd[VulkanStaging::transfer_index], chunk.buffer, buffer.handle, 1, &copy);
    }
    else
    {
        void* mapped = nullptr;
        BEE_VK_CHECK(vmaMapMemory(device.vma_allocator, buffer.allocation, &mapped));
        memcpy(static_cast<u8*>(mapped) + offset, data, size);
        vmaUnmapMemory(device.vma_allocator, buffer.allocation);

        // If the handle is host-coherent we need to flush the range manually
        VkMemoryPropertyFlags mem_type_flags = 0;
        vmaGetMemoryTypeProperties(device.vma_allocator, buffer.allocation_info.memoryType, &mem_type_flags);

        if ((mem_type_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            VkMappedMemoryRange memory_range { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
            memory_range.memory = buffer.allocation_info.deviceMemory;
            memory_range.offset = buffer.allocation_info.offset;
            memory_range.size = buffer.allocation_info.size;

            BEE_VK_CHECK(vkFlushMappedMemoryRanges(device.handle, 1, &memory_range));
        }
    }
}


TextureHandle create_texture(const DeviceHandle& device_handle, const TextureCreateInfo& create_info)
{
    BEE_ASSERT_F(create_info.width > 0 && create_info.height > 0, "Texture cannot be zero-sized");
    BEE_ASSERT_F(create_info.mip_count > 0, "Texture must have at least one mip level");
    BEE_ASSERT_F(create_info.array_element_count > 0, "Texture must have at least one array layer");

    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    u32 queue_family_indices[2] { device.transfer_queue.index, device.graphics_queue.index };

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
    image_info.pQueueFamilyIndices = nullptr;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    if ((create_info.usage & TextureUsage::transfer_dst) != TextureUsage::unknown)
//    {
//        image_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
//        image_info.queueFamilyIndexCount = 2;
//        image_info.pQueueFamilyIndices = queue_family_indices;
//    }
//    else
//    {
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 0; // ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT
//    }

    auto handle = thread.textures.allocate();
    auto& texture = thread.textures[handle];
    memcpy(&texture.create_info, &create_info, sizeof(TextureCreateInfo));

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
        &texture.handle,
        &texture.allocation,
        &texture.allocation_info
    ));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(&device, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture.handle, create_info.debug_name);
    }

    return handle;
}

void destroy_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(texture_handle);
    auto& texture = thread.textures[texture_handle];

    // swapchain images are destroyed with their owning swapchain
    if (texture.swapchain < 0)
    {
        vmaDestroyImage(device.vma_allocator, texture.handle, texture.allocation);
        thread.textures.deallocate(texture_handle);
    }
}

void update_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle, const void* data, const Offset& offset, const Extent& extent, const u32 mip_level, const u32 element)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto& texture = device.textures_get(texture_handle);

    const auto size = extent.width * extent.height * extent.depth * 4;

    VulkanStagingChunk chunk{};
    thread.staging.allocate(size, 1, &chunk);
    memcpy(chunk.data, data, size);

    VkBufferImageCopy copy{};
    copy.bufferOffset = chunk.offset;
    copy.imageSubresource.aspectMask = is_depth_format(texture.create_info.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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
    barrier.oldLayout = texture.layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.handle;
    barrier.subresourceRange.aspectMask = select_access_mask_from_format(texture.create_info.format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = texture.create_info.mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = texture.create_info.array_element_count;

    vkCmdPipelineBarrier(
        chunk.cmd[VulkanStaging::transfer_index],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkCmdCopyBufferToImage(chunk.cmd[VulkanStaging::transfer_index],
        chunk.buffer,
        texture.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy
    );

    if (device.transfer_queue.index != device.graphics_queue.index)
    {
        // Release barrier on the transfer queue
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.srcQueueFamilyIndex = thread.staging.queues[VulkanStaging::transfer_index]->index;
        barrier.dstQueueFamilyIndex = thread.staging.queues[VulkanStaging::graphics_index]->index;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(
            chunk.cmd[VulkanStaging::transfer_index],
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr,
            0, nullptr,
            1, &barrier
        );

        // Acquire barrier on the graphics queue
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = texture.layout;
        const auto dst_stage = select_pipeline_stage_from_access(barrier.dstAccessMask);
        vkCmdPipelineBarrier(
            chunk.cmd[VulkanStaging::graphics_index],
            dst_stage, dst_stage,
            0, 0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    texture.layout = barrier.newLayout;
}

void create_texture_view_internal(VulkanDevice* device, const TextureViewCreateInfo& create_info, VulkanTextureView* dst)
{
    auto& texture = device->textures_get(create_info.texture);

    VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
    view_info.flags = 0;
    view_info.image = texture.handle;
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
    dst->format = texture.create_info.format;
    dst->samples = texture.create_info.sample_count;
    dst->width = texture.create_info.width;
    dst->height = texture.create_info.height;
    dst->depth = texture.create_info.depth;
}

TextureViewHandle create_texture_view(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    const auto handle = thread.texture_views.allocate();
    auto& texture_view = thread.texture_views[handle];

    create_texture_view_internal(&device, create_info, &texture_view);

    return handle;
}

TextureViewHandle create_texture_view_from(const DeviceHandle& device_handle, const TextureHandle& texture_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();
    auto& texture = device.textures_get(texture_handle);
    const auto handle = thread.texture_views.allocate();
    auto& texture_view = thread.texture_views[handle];

    TextureViewCreateInfo info{};
    info.texture = texture_handle;
    info.type = texture.create_info.type;
    info.format = texture.create_info.format;
    info.mip_level_offset = 0;
    info.mip_level_count = texture.create_info.mip_count;
    info.array_element_offset = 0;
    info.array_element_count = texture.create_info.array_element_count;

    create_texture_view_internal(&device, info, &texture_view);

    return handle;
};

void destroy_texture_view(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(texture_view_handle);
    auto& texture_view = thread.texture_views[texture_view_handle];

    BEE_ASSERT(texture_view.handle != VK_NULL_HANDLE);

    vkDestroyImageView(device.handle, texture_view.handle, nullptr);

    thread.texture_views.deallocate(texture_view_handle);
}

FenceHandle create_fence(const DeviceHandle& device_handle, const FenceState initial_state)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

    VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
    info.flags = initial_state == FenceState::signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

    const auto handle = thread.fences.allocate();
    auto& fence = thread.fences[handle];
    BEE_VK_CHECK(vkCreateFence(device.handle, &info, nullptr, &fence));
    return handle;
}

void destroy_fence(const DeviceHandle& device_handle, const FenceHandle& fence_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(fence_handle);
    auto& fence = thread.fences.deallocate(fence_handle);
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
        BEE_DELETE(system_allocator(), descriptor_pool.value);
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
        pool_keyval = descriptor_pools->pools.insert(layout, BEE_NEW(system_allocator(), VulkanDescriptorPool));

        pool_keyval->value->size_count = layout.resources.size;
        pool_keyval->value->layout = device->descriptor_set_layout_cache.get_or_create(layout);

        // initialize the pool sizes
        for (u32 i = 0; i < layout.resources.size; ++i)
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
    const auto handle = thread.resource_bindings.allocate();
    auto& binding = thread.resource_bindings[handle];
    binding.allocated_frame = device.current_frame;
    binding.next = nullptr;
    binding.update_frequency = create_info.update_frequency;
    binding.layout = *create_info.layout;
    binding.set = VK_NULL_HANDLE;
    binding.pool = pool;

    // bindings with frame/draw update frequencies are allocated when binding in the command buffer
    // so we only need to allocate a descriptor up front for persistent bindings
    if (create_info.update_frequency == ResourceBindingUpdateFrequency::persistent)
    {
        VkDescriptorSetAllocateInfo set_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
        set_info.descriptorPool = pool->handle;
        set_info.descriptorSetCount = 1;
        set_info.pSetLayouts = &pool->layout;

        BEE_VK_CHECK(vkAllocateDescriptorSets(device.handle, &set_info, &binding.set));
    }

    return handle;
}

void destroy_resource_binding(const DeviceHandle& device_handle, const ResourceBindingHandle& resource_binding_handle)
{
    auto& device = validate_device(device_handle);
    auto& binding = device.resource_bindings_get(resource_binding_handle);

    if (binding.update_frequency == ResourceBindingUpdateFrequency::persistent)
    {
        auto& thread = device.get_thread();
        if (thread.static_resource_binding_pending_deletes == nullptr)
        {
            thread.static_resource_binding_pending_deletes = &binding;
        }
        else
        {
            thread.static_resource_binding_pending_deletes->next = &binding;
        }

        binding.next = nullptr;
    }
    else
    {
        auto& thread = device.get_thread(resource_binding_handle);
        thread.resource_bindings.deallocate(resource_binding_handle);
    }
}

void allocate_dynamic_binding(VulkanDevice* device, VulkanResourceBinding* binding)
{
    auto* pool = get_or_create_descriptor_pool(device, binding->update_frequency, binding->layout);
    VkDescriptorSetAllocateInfo set_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
    set_info.descriptorPool = binding->pool->handle;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &pool->layout;

    BEE_VK_CHECK(vkAllocateDescriptorSets(device->handle, &set_info, &binding->set));

    binding->allocated_frame = device->current_frame;
}

void update_resource_binding(const DeviceHandle& device_handle, const ResourceBindingHandle& binding_handle, const i32 count, const ResourceBindingUpdate* updates)
{
    auto& device = validate_device(device_handle);
    auto& binding = device.resource_bindings_get(binding_handle);

    if (binding.set == VK_NULL_HANDLE && binding.update_frequency != ResourceBindingUpdateFrequency::persistent)
    {
        allocate_dynamic_binding(&device, &binding);
    }

    struct DescriptorWrite
    {
        VkDescriptorImageInfo*     pImageInfo { nullptr };
        VkDescriptorBufferInfo*    pBufferInfo { nullptr };
        VkBufferView*              pTexelBufferView { nullptr };
    };

    auto* writes = BEE_ALLOCA_ARRAY(VkWriteDescriptorSet, count);
    auto* elements = BEE_ALLOCA_ARRAY(DescriptorWrite, count);

    for (int i = 0; i < count; ++i)
    {
        const auto resource_type = binding.layout.resources[updates[i].binding].type;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr;
        writes[i].dstSet = binding.set;
        writes[i].dstBinding = updates[i].binding;
        writes[i].dstArrayElement = updates[i].first_element;
        writes[i].descriptorCount = updates[i].element_count;
        writes[i].descriptorType = convert_resource_binding_type(resource_type);
        switch (resource_type)
        {
            case ResourceBindingType::sampler:
            case ResourceBindingType::combined_texture_sampler:
            case ResourceBindingType::sampled_texture:
            case ResourceBindingType::storage_texture:
            case ResourceBindingType::input_attachment:
            {
                elements[i].pImageInfo = BEE_ALLOCA_ARRAY(VkDescriptorImageInfo, writes[i].descriptorCount);

                if (is_sampler_binding(resource_type))
                {
                    for (u32 element = 0; element < writes[i].descriptorCount; ++element)
                    {
                        elements[i].pImageInfo[element].sampler = device.samplers_get(updates[i].textures[element].sampler);
                    }
                }

                if (is_texture_binding(resource_type))
                {
                    for (u32 element = 0; element < writes[i].descriptorCount; ++element)
                    {
                        auto& texture_view = device.texture_views_get(updates[i].textures[element].texture);
                        auto& texture = device.textures_get(texture_view.viewed_texture);
                        elements[i].pImageInfo[element].imageView = texture_view.handle;
                        elements[i].pImageInfo[element].imageLayout = texture.layout;
                    }
                }

                writes[i].pImageInfo = elements[i].pImageInfo;
                break;
            }
            case ResourceBindingType::uniform_buffer:
            case ResourceBindingType::storage_buffer:
            case ResourceBindingType::dynamic_uniform_buffer:
            case ResourceBindingType::dynamic_storage_buffer:
            {
                elements[i].pBufferInfo = BEE_ALLOCA_ARRAY(VkDescriptorBufferInfo, writes[i].descriptorCount);

                for (u32 element = 0; element < writes[i].descriptorCount; ++element)
                {
                    elements[i].pBufferInfo[element].buffer = device.buffers_get(updates[i].buffers[element].buffer).handle;
                    elements[i].pBufferInfo[element].offset = updates[i].buffers[element].offset;
                    elements[i].pBufferInfo[element].range = updates[i].buffers[element].size == limits::max<u32>() ? VK_WHOLE_SIZE : updates[i].buffers[element].size;
                }

                writes[i].pBufferInfo = elements[i].pBufferInfo;
                break;
            }
            case ResourceBindingType::uniform_texel_buffer:
            case ResourceBindingType::storage_texel_buffer:
            default:
            {
                BEE_UNREACHABLE("Invalid or unimplemented resource binding type");
                break;
            }
        }
    }

    vkUpdateDescriptorSets(device.handle, count, writes, 0, nullptr);
}

SamplerHandle create_sampler(const DeviceHandle& device_handle, const SamplerCreateInfo& info)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread();

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

    const auto handle = thread.samplers.allocate();
    auto& sampler = thread.samplers[handle];
    BEE_VK_CHECK(vkCreateSampler(device.handle, &vkinfo, nullptr, &sampler));

    return handle;
};

void destroy_sampler(const DeviceHandle& device_handle, const SamplerHandle& sampler_handle)
{
    auto& device = validate_device(device_handle);
    auto& thread = device.get_thread(sampler_handle);
    auto* sampler = thread.samplers.deallocate(sampler_handle);
    vkDestroySampler(device.handle, sampler, nullptr);
}


} // namespace bee

extern void bee_load_command_backend(bee::GpuCommandBackend* api);

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_platform = static_cast<bee::PlatformModule*>(loader->get_module(BEE_PLATFORM_MODULE_NAME));
    bee::g_backend = loader->get_static<bee::VulkanBackend>("BeeVulkanBackend");

    bee::g_backend->api.init = bee::init;
    bee::g_backend->api.destroy = bee::destroy;
    bee::g_backend->api.get_api = bee::get_api;
    bee::g_backend->api.get_name = bee::get_name;
    bee::g_backend->api.is_initialized = bee::is_initialized;
    bee::g_backend->api.get_command_backend = bee::get_command_backend;
    bee::g_backend->api.enumerate_physical_devices = bee::enumerate_physical_devices;
    bee::g_backend->api.create_device = bee::create_device;
    bee::g_backend->api.destroy_device = bee::destroy_device;
    bee::g_backend->api.device_wait = bee::device_wait;
    bee::g_backend->api.submissions_wait = bee::submissions_wait;
    bee::g_backend->api.create_swapchain = bee::create_swapchain;
    bee::g_backend->api.destroy_swapchain = bee::destroy_swapchain;
    bee::g_backend->api.acquire_swapchain_texture = bee::acquire_swapchain_texture;
    bee::g_backend->api.get_swapchain_texture_view = bee::get_swapchain_texture_view;
    bee::g_backend->api.get_swapchain_extent = bee::get_swapchain_extent;
    bee::g_backend->api.get_swapchain_texture_format = bee::get_swapchain_texture_format;
    bee::g_backend->api.get_texture_format = bee::get_texture_format;
    bee::g_backend->api.submit = bee::submit;
    bee::g_backend->api.present = bee::present;
    bee::g_backend->api.commit_frame = bee::commit_frame;
    bee::g_backend->api.get_current_frame = bee::get_current_frame;

    // Resource functions
    bee::g_backend->api.allocate_command_buffer = bee::allocate_command_buffer;
    bee::g_backend->api.create_render_pass = bee::create_render_pass;
    bee::g_backend->api.destroy_render_pass = bee::destroy_render_pass;
    bee::g_backend->api.create_shader = bee::create_shader;
    bee::g_backend->api.destroy_shader = bee::destroy_shader;
    bee::g_backend->api.create_buffer = bee::create_buffer;
    bee::g_backend->api.destroy_buffer = bee::destroy_buffer;
    bee::g_backend->api.update_buffer = bee::update_buffer;
    bee::g_backend->api.create_texture = bee::create_texture;
    bee::g_backend->api.destroy_texture = bee::destroy_texture;
    bee::g_backend->api.update_texture = bee::update_texture;
    bee::g_backend->api.create_texture_view = bee::create_texture_view;
    bee::g_backend->api.create_texture_view_from = bee::create_texture_view_from;
    bee::g_backend->api.destroy_texture_view = bee::destroy_texture_view;
    bee::g_backend->api.create_fence = bee::create_fence;
    bee::g_backend->api.destroy_fence = bee::destroy_fence;
    bee::g_backend->api.create_resource_binding = bee::create_resource_binding;
    bee::g_backend->api.destroy_resource_binding = bee::destroy_resource_binding;
    bee::g_backend->api.update_resource_binding = bee::update_resource_binding;
    bee::g_backend->api.create_sampler = bee::create_sampler;
    bee::g_backend->api.destroy_sampler = bee::destroy_sampler;

    bee_load_command_backend(&bee::g_backend->command_backend);

    auto* gpu_module = static_cast<bee::GpuModule*>(loader->get_module(BEE_GPU_MODULE_NAME));

    if (state == bee::PluginState::loading)
    {
        gpu_module->register_backend(&bee::g_backend->api);
    }
    else
    {
        gpu_module->unregister_backend(&bee::g_backend->api);
    }
}
