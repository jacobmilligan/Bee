/*
 *  Vulkan.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Debug.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"
#include "Bee/Graphics/Vulkan/VulkanConvert.hpp"
#include "Bee/Core/Sort.hpp"


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

    log_error("Vulkan validation: %s (%s) %s: %d: %s\n", layer_prefix, object_names[object_type],
              get_flag_string(), msg_code, msg);
    log_stack_trace(LogVerbosity::error, 9);
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

void set_vk_object_tag(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, size_t tag_size, const void* tag)
{
    if (tag == nullptr || tag_size == 0 || object == nullptr)
    {
        return;
    }

    VkDebugMarkerObjectTagInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT };
    info.objectType = object_type;
    info.object = (u64)object;
    info.tagName = 0;
    info.tagSize = tag_size;
    info.pTag = tag;
    BEE_VK_CHECK(vkDebugMarkerSetObjectTagEXT(device, &info));
}

void set_vk_object_name(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, const char* name)
{
    if (name == nullptr || object == nullptr)
    {
        return;
    }

    VkDebugMarkerObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
    info.objectType = object_type;
    info.object = (u64)object;
    info.pObjectName = name;
    BEE_VK_CHECK(vkDebugMarkerSetObjectNameEXT(device, &info));
}

#else

void gpu_set_object_tag(VkDevice /* device */, VkDebugReportObjectTypeEXT /* object_type */, void* /* object */, size_t /* tag_size */, const void* /* tag */)
{
    // no-op
}

void gpu_set_object_name(VkDevice /* device */, VkDebugReportObjectTypeEXT /* object_type */, void* /* object */, const char* /* name */)
{
    // no-op
}

#endif // BEE_DEBUG


/*
 **************************************************
 *
 * # GPU backend
 *
 * Contains the vulkan instance, physical device
 * data, and extensions/debug layers required
 *
 **************************************************
 */
static VulkanBackend g_backend;


#define BEE_GPU_VALIDATE_BACKEND() BEE_ASSERT_F(g_backend.instance != nullptr, "GPU backend has not been initialized")


VulkanDevice& validate_device(const DeviceHandle& device)
{
    BEE_GPU_VALIDATE_BACKEND();
    BEE_ASSERT_F(
        device.id < BEE_GPU_MAX_DEVICES && g_backend.devices[device.id].handle != VK_NULL_HANDLE,
        "GPU device has an invalid ID or is destroyed/uninitialized"
    );
    return g_backend.devices[device.id];
}

VulkanSubmission* enqueue_submission(VulkanDevice* device)
{
    const auto submission_index = device->submit_queue_tail.fetch_add(1, std::memory_order_acquire);
    if (BEE_FAIL(submission_index < BEE_GPU_SUBMIT_QUEUE_SIZE))
    {
        auto expected = submission_index + 1;
        device->submit_queue_tail.compare_exchange_strong(expected, submission_index);
        return nullptr;
    }

    auto submission = &device->submit_queue[device->current_frame][submission_index];
    submission->wait();
    submission->reset(device->handle);
    return submission;
}


/*
 ******************************************
 *
 * Vulkan cached objects
 *
 ******************************************
 */

inline bool framebuffer_attachments_match(const VulkanFramebuffer& framebuffer, const VkImageView* attachments)
{
    for (u32 i = 0; i < framebuffer.key.attachment_count; ++i)
    {
        if (framebuffer.image_views[i] != attachments[i])
        {
            return false;
        }
    }

    return true;
}

VkFramebuffer get_or_create_framebuffer(
    VulkanDevice* device,
    const VulkanFramebufferKey& key,
    VkRenderPass compatible_render_pass,
    const VkImageView* attachments
)
{
    KeyValuePair<u32, VulkanDevice::framebuffer_bucket_t>* bucket = nullptr;
    const auto hash = get_hash(key);

    // use a scoped read lock for checking the cache - this will let us check the cache for essentially free if
    // all threads are reading
    BEE_EXPLICIT_SCOPE
    (
        scoped_rw_read_lock_t lock(device->framebuffer_cache_mutex);

        bucket = device->framebuffer_cache.find(hash);

        if (bucket != nullptr)
        {
            for (auto& framebuffer : bucket->value)
            {
                if (framebuffer_attachments_match(framebuffer, attachments))
                {
                    return framebuffer.handle;
                }
            }
        }
    );

    // lock for writing
    scoped_rw_write_lock_t lock(device->framebuffer_cache_mutex);

    if (bucket == nullptr)
    {
        bucket = device->framebuffer_cache.insert(
            hash,
            std::move(VulkanDevice::framebuffer_bucket_t { 1, system_allocator() })
        );
    }

    bucket->value.emplace_back();
    auto& framebuffer = bucket->value.back();

    // cache off images and key for comparing later
    memcpy(framebuffer.image_views, attachments, sizeof(VkImageView) * key.attachment_count);
    framebuffer.key = key;

    VkFramebufferCreateInfo info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr };
    info.flags = 0;
    info.renderPass = compatible_render_pass;
    info.attachmentCount = key.attachment_count;
    info.pAttachments = framebuffer.image_views;
    info.width = key.width;
    info.height = key.height;
    info.layers = key.layers;

    BEE_VK_CHECK(vkCreateFramebuffer(device->handle, &info, nullptr, &framebuffer.handle));

    return framebuffer.handle;
}

VkDescriptorSetLayout get_or_create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key)
{
    const auto hash = get_hash(key);
    auto layout = device->descriptor_set_layout_cache.find(hash);
    if (layout != nullptr)
    {
        return layout->value;
    }

    auto bindings = FixedArray<VkDescriptorSetLayoutBinding>::with_size(key.resource_count, &device->scratch_allocator);
    for (int i = 0; i < bindings.size(); ++i)
    {
        bindings[i].binding = key.resources[i].binding;
        bindings[i].descriptorType = convert_resource_binding_type(key.resources[i].type);
        bindings[i].descriptorCount = key.resources[i].element_count;
        bindings[i].stageFlags = decode_shader_stage(key.resources[i].shader_stages);
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
    info.flags = 0;
    info.bindingCount = key.resource_count;
    info.pBindings = bindings.data();

    layout = device->descriptor_set_layout_cache.insert(hash, VK_NULL_HANDLE);
    BEE_VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &info, nullptr, &layout->value));
    return layout->value;
}

VkPipelineLayout get_or_create_pipeline_layout(VulkanDevice* device, const VulkanPipelineLayoutKey& key)
{
    const auto hash = get_hash(key);
    auto layout = device->pipeline_layout_cache.find(hash);
    if (layout != nullptr)
    {
        return layout->value;
    }

    auto descriptor_set_layouts = FixedArray<VkDescriptorSetLayout>::with_size(key.resource_layout_count, &device->scratch_allocator);
    for (int i = 0; i < descriptor_set_layouts.size(); ++i)
    {
        descriptor_set_layouts[i] = get_or_create_descriptor_set_layout(device, key.resource_layouts[i]);
    }

    auto push_constants = FixedArray<VkPushConstantRange>::with_size(key.push_constant_range_count, &device->scratch_allocator);
    for (int i = 0; i < push_constants.size(); ++i)
    {
        push_constants[i].stageFlags = decode_shader_stage(key.push_constant_ranges[i].shader_stages);
        push_constants[i].offset = key.push_constant_ranges[i].offset;
        push_constants[i].size = key.push_constant_ranges[i].size;
    }

    VkPipelineLayoutCreateInfo info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
    info.flags = 0;
    info.setLayoutCount = key.resource_layout_count;
    info.pSetLayouts = descriptor_set_layouts.data();
    info.pushConstantRangeCount = key.push_constant_range_count;
    info.pPushConstantRanges = push_constants.data();

    layout = device->pipeline_layout_cache.insert(hash, VK_NULL_HANDLE);
    BEE_VK_CHECK(vkCreatePipelineLayout(device->handle, &info, nullptr, &layout->value));

    return layout->value;
}


/*
 ****************************************
 *
 * # GPU backend API - implementation
 *
 ****************************************
 */
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
    for (u32 pd = 0; pd < device_count; ++pd)
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
        str::format_buffer(info.api_version, static_array_length(info.api_version), "Vulkan %u.%u.%u", major, minor, patch);
    }

    return device_count;
}


/*
 ****************************************
 *
 * # GPU Device - implementation
 *
 ****************************************
 */
DeviceHandle gpu_create_device(const DeviceCreateInfo& create_info)
{
    BEE_GPU_VALIDATE_BACKEND();

    if (g_backend.instance == VK_NULL_HANDLE)
    {
        log_error("Failed to create GPU device: Vulkan instance was VK_NULL_HANDLE");
        return DeviceHandle{};
    }

    const auto is_valid_physical_device_id = create_info.physical_device_id >= 0
                                          && create_info.physical_device_id < g_backend.physical_device_count;
    if (BEE_FAIL_F(is_valid_physical_device_id, "Invalid physical device ID specified in `DeviceCreateInfo`"))
    {
        return DeviceHandle{};
    }

    int device_idx = find_index_if(g_backend.devices, [](const VulkanDevice& d)
    {
        return d.handle == VK_NULL_HANDLE;
    });
    if (BEE_FAIL_F(device_idx >= 0, "Cannot create a new GPU device: Allocated devices has reached BEE_GPU_MAX_DEVICES"))
    {
        return DeviceHandle{};
    }

    auto physical_device = g_backend.physical_devices[create_info.physical_device_id];

    // Query the amount of extensions supported by the GPU
#ifdef BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED
    u32 ext_count = 0;
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, nullptr));

    // Get the actual extension properties for the GPU
    auto supported_extensions = FixedArray<VkExtensionProperties>::with_size(sign_cast<int>(ext_count), temp_allocator());
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, supported_extensions.data()));
    // TODO(Jacob): iterate desired extensions here when possible
#else
    DynamicArray<const char*> device_extensions(temp_allocator());
    for (const auto& ext : VulkanBackend::device_extensions)
    {
        device_extensions.push_back(ext);
    }
#endif // BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED

    auto& device = g_backend.devices[device_idx];
    device.physical_device = physical_device;

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
        device.queues[i].mutex = &device.per_queue_mutex[i];
    }

    VmaAllocatorCreateInfo vma_info{};
    vma_info.device = device.handle;
    vma_info.physicalDevice = physical_device;
    BEE_VK_CHECK(vmaCreateAllocator(&vma_info, &device.vma_allocator));

    // Initialize the staging buffers
    device.staging.init(device.handle, &device.transfer_queue, device.vma_allocator);

    return DeviceHandle(sign_cast<u32>(device_idx));
}

void gpu_destroy_device(const DeviceHandle& handle)
{
    auto& device = validate_device(handle);
    device.staging.destroy();

    BEE_EXPLICIT_SCOPE
    (
        scoped_rw_write_lock_t lock(device.framebuffer_cache_mutex);
        for (auto& bucket : device.framebuffer_cache)
        {
            for (auto& framebuffer : bucket.value)
            {
                vkDestroyFramebuffer(device.handle, framebuffer.handle, nullptr);
            }
        }
    );

    for (auto& desc_set_layout : device.descriptor_set_layout_cache)
    {
        vkDestroyDescriptorSetLayout(device.handle, desc_set_layout.value, nullptr);
    }

    for (auto& pipeline_layout : device.pipeline_layout_cache)
    {
        vkDestroyPipelineLayout(device.handle, pipeline_layout.value, nullptr);
    }

    vmaDestroyAllocator(device.vma_allocator);
    vkDestroyDevice(device.handle, nullptr);
    destruct(&device);

    device.physical_device = VK_NULL_HANDLE;
    device.handle = VK_NULL_HANDLE;
    device.vma_allocator = VK_NULL_HANDLE;
}

void gpu_device_wait(const DeviceHandle& handle)
{
    vkDeviceWaitIdle(validate_device(handle).handle);
}

SwapchainHandle gpu_create_swapchain(const DeviceHandle& device_handle, const SwapchainCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);

    // Create a surface and query its capabilities
    auto surface = gpu_create_wsi_surface(g_backend.instance, create_info.window);
    BEE_ASSERT(surface != VK_NULL_HANDLE);

    /*
     * If we've never found the present queue for the device we have to do it here rather than in create_device
     * as it requires a valid surface to query.
     */
    if (device.present_queue == VulkanQueue::invalid_queue_index)
    {
        // Prefers graphics/present combined queue over other combinations - first queue is always the graphics queue
        VkBool32 supports_present = VK_FALSE;
        for (const auto& queue : device.queues)
        {
            BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
                device.physical_device,
                device.graphics_queue.index,
                surface,
                &supports_present
            ));

            if (supports_present == VK_TRUE)
            {
                device.present_queue = queue.index;
                break;
            }
        }
    }

    // Get the surface capabilities and ensure it supports all the things we need
    VkSurfaceCapabilitiesKHR surface_caps{};
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical_device, surface, &surface_caps));

    // Get supported formats
    u32 format_count = 0;
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &format_count, nullptr));
    auto formats = FixedArray<VkSurfaceFormatKHR>::with_size(sign_cast<i32>(format_count), &device.scratch_allocator);
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device, surface, &format_count, formats.data()));

    // Get supported present modes
    u32 present_mode_count = VK_PRESENT_MODE_RANGE_SIZE_KHR;
    VkPresentModeKHR present_modes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
    BEE_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device, surface, &present_mode_count, present_modes));

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

    // Create the vk_handle
    VkSwapchainCreateInfoKHR swapchain_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_info.flags = 0;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = selected_format.format;
    swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent.width = create_info.texture_extent.width;
    swapchain_info.imageExtent.height = create_info.texture_extent.height;
    swapchain_info.imageArrayLayers = create_info.texture_array_layers;
    swapchain_info.imageUsage = decode_image_usage(create_info.texture_usage);
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = nullptr;
    swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // no pre-transform
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore surface alpha channel
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE; // allows optimal presentation of pixels clipped in the surface by other OS windows etc.
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR vk_handle;
    BEE_VK_CHECK(vkCreateSwapchainKHR(device.handle, &swapchain_info, nullptr, &vk_handle));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device.handle, VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, vk_handle, create_info.debug_name);
    }

    // Setup the swapchain images
    u32 swapchain_image_count = 0;
    BEE_VK_CHECK(vkGetSwapchainImagesKHR(device.handle, vk_handle, &swapchain_image_count, nullptr));
    auto swapchain_images = FixedArray<VkImage>::with_size(sign_cast<i32>(swapchain_image_count), &device.scratch_allocator);
    BEE_VK_CHECK(vkGetSwapchainImagesKHR(device.handle, vk_handle, &swapchain_image_count, swapchain_images.data()));

    const auto created_handle = device.swapchains.allocate();
    auto& swapchain = device.swapchains[created_handle];
    swapchain.handle = vk_handle;
    swapchain.surface = surface;
    swapchain.images = FixedArray<TextureHandle>::with_size(image_count);
    swapchain.image_views = FixedArray<TextureViewHandle>::with_size(image_count);
    swapchain.extent = create_info.texture_extent;

    str::format_buffer(swapchain.id_string, static_array_length(swapchain.id_string), "handle:%u", created_handle.id);
    set_vk_object_name(
        device.handle,
        VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT,
        vk_handle,
        create_info.debug_name == nullptr ? swapchain.id_string : create_info.debug_name
    );
    /*
     * Insert a texture handle for each of the swapchain images to use with external code and create a texture view
     * for each one
     */
    TextureViewCreateInfo view_info;
    view_info.type = TextureType::tex2d;
    view_info.format = create_info.texture_format;
    view_info.mip_level_count = 1;
    view_info.mip_level_offset = 0;
    view_info.array_element_offset = 0;
    view_info.array_element_count = 1;

    for (int si = 0; si < swapchain_images.size(); ++si)
    {
        swapchain.images[si] = device.textures.allocate();
        auto& texture = device.textures[swapchain.images[si]];
        texture.swapchain_handle = created_handle;
        texture.width = swapchain_info.imageExtent.width;
        texture.height = swapchain_info.imageExtent.height;
        texture.layers = swapchain_info.imageArrayLayers;
        texture.levels = 1;
        texture.samples = VK_SAMPLE_COUNT_1_BIT;
        texture.format = create_info.texture_format;
        texture.handle = swapchain_images[si];
        set_vk_object_name(device.handle, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture.handle, "Swapchain image");

        // Create a texture view as well
        view_info.texture = swapchain.images[si];
        view_info.debug_name = "Swapchain texture view";
        swapchain.image_views[si] = gpu_create_texture_view(device_handle, view_info);
        auto& texture_view = device.texture_views[swapchain.image_views[si]];
        texture_view.swapchain_handle = created_handle;
    }

    // Create image available and render finished semaphores
    VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    sem_info.flags = 0;
    for (int frame_idx = 0; frame_idx < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame_idx)
    {
        BEE_VK_CHECK(vkCreateSemaphore(device.handle, &sem_info, nullptr, &swapchain.acquire_semaphore[frame_idx]));
        BEE_VK_CHECK(vkCreateSemaphore(device.handle, &sem_info, nullptr, &swapchain.render_semaphore[frame_idx]));
    }

    return created_handle;
}

void gpu_destroy_swapchain(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle];

    BEE_VK_CHECK(vkDeviceWaitIdle(device.handle));

    for (int i = 0; i < swapchain.images.size(); ++i)
    {
        if (swapchain.image_views[i].is_valid())
        {
            gpu_destroy_texture_view(device_handle, swapchain.image_views[i]);
        }

        if (swapchain.images[i].is_valid())
        {
            gpu_destroy_texture(device_handle, swapchain.images[i]);
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
    vkDestroySurfaceKHR(g_backend.instance, swapchain.surface, nullptr);
    device.swapchains.deallocate(swapchain_handle);
}

i32 get_or_acquire_swapchain_image(VulkanDevice* device, VulkanSwapchain* swapchain)
{
    /*
     * vkAcquireNextImageKHR can access a swapchain across multiple threads as long as it's externally
     * synchronized
     * see: Vulkan Spec - 2.6. Threading Behavior
     */
    scoped_recursive_spinlock_t lock(swapchain->mutex);

    if (swapchain->pending_image_acquire)
    {
        BEE_VK_CHECK(vkAcquireNextImageKHR(
            device->handle,
            swapchain->handle,
            limits::max<u64>(),
            swapchain->acquire_semaphore[device->current_frame],
            VK_NULL_HANDLE,
            &swapchain->current_image // get the next image index
        ));

        swapchain->pending_image_acquire = false;
    }

    return swapchain->current_image;
}

TextureHandle gpu_acquire_swapchain_texture(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle];
    const auto index = get_or_acquire_swapchain_image(&device, &swapchain);
    return swapchain.images[index];
}

TextureViewHandle gpu_get_swapchain_texture_view(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle];
    return swapchain.image_views[swapchain.current_image];
}

Extent gpu_get_swapchain_extent(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle];
    return swapchain.extent;
}

RenderPassHandle gpu_create_render_pass(const DeviceHandle& device_handle, const RenderPassCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);

    auto subpasses = FixedArray<VkSubpassDescription>::with_size(create_info.subpass_count, &device.scratch_allocator);
    auto attachments = FixedArray<VkAttachmentDescription>::with_size(create_info.attachment_count, &device.scratch_allocator);
    auto subpass_deps = FixedArray<VkSubpassDependency>::with_size(create_info.subpass_count, &device.scratch_allocator);

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
        auto& bee_attachment = create_info.attachments[a];

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

    DynamicArray<VkAttachmentReference> attachment_refs(&device.scratch_allocator);

    for (int sp = 0; sp < subpasses.size(); ++sp)
    {
        auto& subpass = subpasses[sp];
        auto& bee_subpass = create_info.subpasses[sp];

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

        auto input_attachments = attachment_refs.data() + this_subpass_begin;
        auto color_attachments = input_attachments + bee_subpass.input_attachment_count;
        auto resolve_attachments = color_attachments + bee_subpass.color_attachment_count;
        auto depth_stencil_attachment = resolve_attachments + bee_subpass.resolve_attachment_count;

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

            auto& prev_subpass = create_info.subpasses[sp - 1];

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

    const auto handle = device.render_passes.allocate();
    auto& render_pass = device.render_passes[handle];

    render_pass.create_info = create_info;

    BEE_VK_CHECK(vkCreateRenderPass(device.handle, &vk_info, nullptr, &render_pass.handle));

    return handle;
}

void gpu_destroy_render_pass(const DeviceHandle& device_handle, const RenderPassHandle& render_pass_handle)
{
    auto& device = validate_device(device_handle);
    auto& render_pass = device.render_passes[render_pass_handle];
    vkDestroyRenderPass(device.handle, render_pass.handle, nullptr);
}

ShaderHandle gpu_create_shader(const DeviceHandle& device_handle, const ShaderCreateInfo& info)
{
    auto& device = validate_device(device_handle);

    VkShaderModuleCreateInfo vk_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr };
    vk_info.flags = 0;
    vk_info.codeSize = info.code_size;
    vk_info.pCode = reinterpret_cast<const u32*>(info.code);

    const auto handle = device.shaders.allocate();
    auto& shader = device.shaders[handle];

    shader.entry = info.entry;

    BEE_VK_CHECK(vkCreateShaderModule(device.handle, &vk_info, nullptr, &shader.handle));

    return handle;
}

void gpu_destroy_shader(const DeviceHandle& device_handle, const ShaderHandle& shader_handle)
{
    auto& device = validate_device(device_handle);
    auto& shader = device.shaders[shader_handle];
    vkDestroyShaderModule(device.handle, shader.handle, nullptr);
    device.shaders.deallocate(shader_handle);
}

PipelineStateHandle gpu_create_pipeline_state(const DeviceHandle& device_handle, const PipelineStateCreateInfo& create_info)
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

    DynamicArray<VkPipelineShaderStageCreateInfo> stages(&device.scratch_allocator);

    for (const auto& stage : shaders)
    {
        if (!stage.handle.is_valid())
        {
            continue;
        }

        auto& shader = device.shaders[stage.handle];

        stages.emplace_back();

        auto& stage_info = stages.back();
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext = nullptr;
        stage_info.flags = 0;
        stage_info.stage = stage.flags;
        stage_info.pName = shader.entry.c_str();
        stage_info.pSpecializationInfo = nullptr;
    }

    /*
     * Vertex input state
     */
    auto vertex_binding_descs = FixedArray<VkVertexInputBindingDescription>::with_size(create_info.vertex_description.layout_count, &device.scratch_allocator);
    auto vertex_attribute_descs = FixedArray<VkVertexInputAttributeDescription>::with_size(create_info.vertex_description.attribute_count, &device.scratch_allocator);

    for (int b = 0; b < vertex_binding_descs.size(); ++b)
    {
        auto& vk_desc = vertex_binding_descs[b];
        auto& layout = create_info.vertex_description.layouts[b];

        vk_desc.binding = layout.index;
        vk_desc.inputRate = convert_step_function(layout.step_function);
        vk_desc.stride = layout.stride;
    }

    for (int a = 0; a < vertex_attribute_descs.size(); ++a)
    {
        auto& vk_desc = vertex_attribute_descs[a];
        auto& attr = create_info.vertex_description.attributes[a];

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
        &device.scratch_allocator
    );

    for (int i = 0; i < color_blend_attachments.size(); ++i)
    {
        auto& vk_state = color_blend_attachments[i];
        auto& state = create_info.color_blend_states[i];

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
    auto pipeline_layout = get_or_create_pipeline_layout(&device, pipeline_layout_key);

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
    info.renderPass = device.render_passes[create_info.compatible_render_pass].handle;
    info.subpass = create_info.subpass_index;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;

    // phew, that was a lot of typing - I think we earned ourselves a nice graphics pipeline object
    const auto handle = device.pipelines.allocate();
    auto& pipeline = device.pipelines[handle];

    BEE_VK_CHECK(vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.handle));

    return handle;
}

void gpu_destroy_pipeline_state(const DeviceHandle& device_handle, const PipelineStateHandle& pipeline_handle)
{
    auto& device = validate_device(device_handle);
    auto& pipeline = device.pipelines[pipeline_handle];

    vkDestroyPipeline(device.handle, pipeline.handle, nullptr);
}

BufferHandle gpu_create_buffer(const DeviceHandle& device_handle, const BufferCreateInfo& create_info)
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

    const auto handle = device.buffers.allocate(create_info.type, create_info.memory_usage, create_info.size);
    auto& buffer = device.buffers[handle];

    BEE_VK_CHECK(vmaCreateBuffer(
        device.vma_allocator,
        &vk_info,
        &vma_info,
        &buffer.handle,
        &buffer.allocation,
        &buffer.allocation_info
    ));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device.handle, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, buffer.handle, create_info.debug_name);
    }

    return handle;
}

void gpu_destroy_buffer(const DeviceHandle& device_handle, const BufferHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& buffer = device.buffers[handle];
    BEE_ASSERT(buffer.handle != VK_NULL_HANDLE);
    BEE_ASSERT(buffer.allocation != VK_NULL_HANDLE);

    vmaDestroyBuffer(device.vma_allocator, buffer.handle, buffer.allocation);
    device.buffers.deallocate(handle);
}

void gpu_update_buffer(const DeviceHandle& device_handle, const BufferHandle& buffer_handle, const void* data, const size_t offset, const size_t size)
{
    auto& device = validate_device(device_handle);
    auto& buffer = device.buffers[buffer_handle];

    if (buffer.usage == DeviceMemoryUsage::gpu_only)
    {
        VulkanStagingChunk chunk{};
        device.staging.allocate(size, 1, &chunk);

        memcpy(chunk.data, data, size);

        VkBufferCopy copy{};
        copy.srcOffset = chunk.offset;
        copy.dstOffset = offset;
        copy.size = size;

        vkCmdCopyBuffer(chunk.cmd, chunk.buffer, buffer.handle, 1, &copy);
    }
    else
    {
        void* mapped = nullptr;
        BEE_VK_CHECK(vmaMapMemory(device.vma_allocator, buffer.allocation, &mapped));
        memcpy(static_cast<u8*>(mapped) + offset, data, size);
        vmaUnmapMemory(device.vma_allocator, buffer.allocation);

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

TextureHandle gpu_create_texture(const DeviceHandle& device_handle, const TextureCreateInfo& create_info)
{
    BEE_ASSERT_F(create_info.width > 0 && create_info.height > 0, "Texture cannot be zero-sized");
    BEE_ASSERT_F(create_info.mip_count > 0, "Texture must have at least one mip level");
    BEE_ASSERT_F(create_info.array_element_count > 0, "Texture must have at least one array layer");

    auto& device = validate_device(device_handle);

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

    auto handle = device.textures.allocate();
    auto& texture = device.textures[handle];
    texture.width = create_info.width;
    texture.height = create_info.height;
    texture.layers = create_info.array_element_count;
    texture.levels = create_info.mip_count;
    texture.format = create_info.format;
    texture.samples = create_info.sample_count;
    texture.usage = create_info.usage;

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
        set_vk_object_name(device.handle, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, texture.handle, create_info.debug_name);
    }

    return handle;
}

void gpu_destroy_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle)
{
    auto& device = validate_device(device_handle);
    auto& texture = device.textures[texture_handle];
    BEE_ASSERT(texture.handle != VK_NULL_HANDLE);
    // swapchain images are destroyed with their owning swapchain
    if (!texture.swapchain_handle.is_valid())
    {
        vmaDestroyImage(device.vma_allocator, texture.handle, texture.allocation);
    }
    device.textures.deallocate(texture_handle);
}

void gpu_update_texture(const DeviceHandle& device_handle, const TextureHandle& texture_handle, const void* data, const Offset& offset, const Extent& extent, const u32 mip_level, const u32 element)
{
    auto& device = validate_device(device_handle);
    auto& texture = device.textures[texture_handle];

    VulkanStagingChunk chunk{};
    device.staging.allocate(texture.width * texture.height * texture.depth, 1, &chunk);

    const auto size = extent.width * extent.height * extent.depth;
    memcpy(chunk.data, data, size);

    VkBufferImageCopy copy{};
    copy.bufferOffset = chunk.offset;
    copy.imageSubresource.aspectMask = is_depth_format(texture.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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
    barrier.image = texture.handle;
    barrier.subresourceRange.aspectMask = select_access_mask_from_format(texture.format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = texture.levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = texture.layers;

    vkCmdPipelineBarrier(
        chunk.cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkCmdCopyBufferToImage(chunk.cmd, chunk.buffer, texture.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

TextureViewHandle gpu_create_texture_view(const DeviceHandle& device_handle, const TextureViewCreateInfo& create_info)
{
    if (BEE_FAIL_F(create_info.texture.is_valid(), "Invalid texture handle given as source texture to TextureViewCreateInfo"))
    {
        return TextureViewHandle{};
    }

    auto& device = validate_device(device_handle);
    auto& texture = device.textures[create_info.texture];

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
    BEE_VK_CHECK(vkCreateImageView(device.handle, &view_info, nullptr, &img_view));

    if (create_info.debug_name != nullptr)
    {
        set_vk_object_name(device.handle, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, img_view, create_info.debug_name);
    }

    const auto handle = device.texture_views.allocate();
    auto& texture_view = device.texture_views[handle];
    texture_view.handle = img_view;
    texture_view.viewed_texture = create_info.texture;
    texture_view.format = texture.format;
    texture_view.samples = texture.samples;
    return handle;
}

void gpu_destroy_texture_view(const DeviceHandle& device_handle, const TextureViewHandle& texture_view_handle)
{
    auto& device = validate_device(device_handle);
    auto& texture_view = device.texture_views[texture_view_handle];
    BEE_ASSERT(texture_view.handle != VK_NULL_HANDLE);
    vkDestroyImageView(device.handle, texture_view.handle, nullptr);
    device.texture_views.deallocate(texture_view_handle);
}

CommandPoolHandle gpu_create_command_pool(const DeviceHandle& device_handle, const CommandPoolCreateInfo& create_info)
{
    auto& device = validate_device(device_handle);
    auto vk_info = VkCommandPoolCreateInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    vk_info.flags = convert_command_pool_hint(create_info.pool_hint);

    const auto handle = device.command_pools.allocate();
    auto& pool = device.command_pools[handle];

    new (&pool.allocator) PoolAllocator(sizeof(CommandBuffer), alignof(CommandBuffer), 0);

    for_each_flag(create_info.used_queues_hint, [&](const QueueType queue_type)
    {
        switch (queue_type)
        {
            case QueueType::graphics:
            {
                vk_info.queueFamilyIndex = device.graphics_queue.index;
                break;
            }
            case QueueType::compute:
            {
                vk_info.queueFamilyIndex = device.compute_queue.index;
                break;
            }
            case QueueType::transfer:
            {
                vk_info.queueFamilyIndex = device.transfer_queue.index;
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Cannot create a command pool with no invalid type");
            }
        }

        auto vk_pool = &pool.per_queue_pools[queue_type_index(queue_type)];
        BEE_VK_CHECK(vkCreateCommandPool(device.handle, &vk_info, nullptr, &vk_pool->handle));
    });

    return handle;
}

void gpu_destroy_command_pool(const DeviceHandle& device_handle, const CommandPoolHandle& handle)
{
    // frees all pooled NativeCommandBuffer memory
    gpu_reset_command_pool(device_handle, handle);

    auto& device = validate_device(device_handle);
    auto& pool = device.command_pools[handle];

    for (auto& per_queue_pool : pool.per_queue_pools)
    {
        if (per_queue_pool.handle != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device.handle, per_queue_pool.handle, nullptr);
        }
    }

    device.command_pools.deallocate(handle);
}

void gpu_reset_command_pool(const DeviceHandle& device_handle, const CommandPoolHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& pool = device.command_pools[handle];
    for (auto& per_queue_pool : pool.per_queue_pools)
    {
        if (per_queue_pool.handle != VK_NULL_HANDLE)
        {
            for (auto& cmd_buffer : per_queue_pool.command_buffers)
            {
                // don't deallocate the vk command buffer because vkResetCommandPool cleans up cmd buffer resources
                BEE_DELETE(pool.allocator, cmd_buffer);
            }

            per_queue_pool.command_buffers.clear();

            BEE_VK_CHECK(vkResetCommandPool(device.handle, per_queue_pool.handle, 0));
        }
    }
}

FenceHandle gpu_create_fence(const DeviceHandle& device_handle, const FenceState initial_state)
{
    auto& device = validate_device(device_handle);

    VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
    info.flags = initial_state == FenceState::signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

    const auto handle = device.fences.allocate();
    auto& fence = device.fences[handle];

    BEE_VK_CHECK(vkCreateFence(device.handle, &info, nullptr, &fence));

    return handle;
}

void gpu_destroy_fence(const DeviceHandle& device_handle, const FenceHandle& handle)
{
    auto& device = validate_device(device_handle);
    auto& fence = device.fences[handle];

    vkDestroyFence(device.handle, fence, nullptr);
    device.fences.deallocate(handle);
}

void gpu_wait_for_fence(const DeviceHandle& device_handle, const FenceHandle& fence, const u64 timeout)
{
    gpu_wait_for_fences(device_handle, 1, &fence, FenceWaitType::all, timeout);
}

bool gpu_wait_for_fences(const DeviceHandle& device_handle, const u32 count, const FenceHandle* fence_handles, const FenceWaitType wait_type, const u64 timeout)
{
    auto& device = validate_device(device_handle);
    auto fences = BEE_ALLOCA_ARRAY(VkFence, count);
    for (u32 f = 0; f < count; ++f)
    {
        fences[f] = device.fences[fence_handles[f]];
    }

    const auto result = vkWaitForFences(device.handle, count, fences, wait_type == FenceWaitType::all, timeout);
    BEE_ASSERT_F(result == VK_SUCCESS || result == VK_TIMEOUT, "Vulkan: %s", vk_result_string(result));
    return result == VK_SUCCESS;
}

void gpu_reset_fences(const DeviceHandle& device_handle, const u32 count, const FenceHandle* fence_handles)
{
    auto& device = validate_device(device_handle);
    auto fences = BEE_ALLOCA_ARRAY(VkFence, count);

    for (u32 f = 0; f < count; ++f)
    {
        fences[f] = device.fences[fence_handles[f]];
    }

    BEE_VK_CHECK(vkResetFences(device.handle, count, fences));
}

void gpu_reset_fence(const DeviceHandle& device_handle, const FenceHandle& fence_handle)
{
    gpu_reset_fences(device_handle, 1, &fence_handle);
}

FenceState gpu_get_fence_state(const DeviceHandle& device_handle, const FenceHandle& fence_handle)
{
    auto& device = validate_device(device_handle);
    auto& fence = device.fences[fence_handle];

    const auto status = vkGetFenceStatus(device.handle, fence);

    switch (status)
    {
        case VK_SUCCESS:
        {
            return FenceState::signaled;
        }
        case VK_NOT_READY:
        {
            return FenceState::unsignaled;
        }
        case VK_ERROR_DEVICE_LOST:
        {
            return FenceState::device_lost;
        }
        default:
        {
            break;
        }
    }

    return FenceState::unknown;
}

void VulkanQueue::submit_threadsafe(const u32 submit_count, const VkSubmitInfo* submits, VkFence fence)
{
    /*
     * vkQueueSubmit can access a queue across multiple threads as long as it's externally
     * synchronized i.e. with a mutex
     * see: Vulkan Spec - 2.6. Threading Behavior
     */
    scoped_recursive_spinlock_t lock(*mutex);
    BEE_VK_CHECK(vkQueueSubmit(handle, submit_count, submits, fence));
}

void VulkanQueue::present_threadsafe(const VkPresentInfoKHR* present_info)
{
    /*
     * vkQueuePresentKHR can access a queue across multiple threads as long as it's externally
     * synchronized i.e. with a mutex
     * see: Vulkan Spec - 2.6. Threading Behavior
     */
    scoped_recursive_spinlock_t lock(*mutex);
    BEE_VK_CHECK(vkQueuePresentKHR(handle, present_info));
}

struct QueueSubmit
{
    VkSubmitInfo                    info { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
    DynamicArray<VkCommandBuffer>   command_buffers;

    QueueSubmit() = default;

    QueueSubmit(VulkanDevice* device, Allocator* allocator)
        : command_buffers(allocator)
    {}

    void push_command_buffer(const CommandBuffer* cmd)
    {
        command_buffers.push_back(cmd->native()->handle);
        ++info.commandBufferCount;
        info.pCommandBuffers = command_buffers.data();
    }
};


void submit_job(VulkanDevice* device, const FenceHandle& fence, FixedArray<const CommandBuffer*> command_buffers)
{
    static constexpr VkPipelineStageFlags swapchain_wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    QueueSubmit submissions[vk_max_queues];
    for (auto& submission : submissions)
    {
        new (&submission) QueueSubmit(device, temp_allocator());
    }

    // Gather all the command buffers into per-queue submissions
    for (auto& command_buffer : command_buffers)
    {
        auto native_cmd = command_buffer->native();
        auto& submission = submissions[native_cmd->queue];

        // we have to add a semaphore if the command buffer is targeting the swapchain
        if (native_cmd->target_swapchain.is_valid())
        {
            auto& swapchain = device->swapchains[native_cmd->target_swapchain];

            if (BEE_FAIL_F(!swapchain.pending_image_acquire, "Swapchain cannot be rendered to without first acquiring its current texture"))
            {
                return;
            }

            submission.info.waitSemaphoreCount = 1;
            submission.info.pWaitSemaphores = &swapchain.acquire_semaphore[device->current_frame];
            submission.info.pWaitDstStageMask = &swapchain_wait_stage;
            submission.info.signalSemaphoreCount = 1;
            submission.info.pSignalSemaphores = &swapchain.render_semaphore[device->current_frame];
        }

        submission.push_command_buffer(command_buffer);
    }

    auto& vk_fence = device->fences[fence];

    for (int queue = 0; queue < vk_max_queues; ++queue)
    {
        auto& submission = submissions[queue];
        if (submission.command_buffers.empty())
        {
            continue;
        }

        device->queues[queue].submit_threadsafe(1, &submission.info, vk_fence);
    }
}

void gpu_submit(JobGroup* wait_handle, const DeviceHandle& device_handle, const SubmitInfo& info)
{
    if (info.command_buffer_count == 0)
    {
        log_warning("GPU warning: created a submit request with 0 command buffers");
        return;
    }

    BEE_ASSERT_F(info.command_buffers != nullptr, "`command_buffers` must point to an array of `command_buffer_count` GpuCommandBuffer pointers");

    auto& device = validate_device(device_handle);

    auto cmds = FixedArray<const CommandBuffer*>::with_size(info.command_buffer_count, temp_allocator());
    memcpy(cmds.data(), info.command_buffers, sizeof(CommandBuffer*) * info.command_buffer_count);

    auto job = create_job(submit_job, &device, info.fence, std::move(cmds));
    job_schedule(wait_handle, job);
}

void gpu_submit(const DeviceHandle& device_handle, const SubmitInfo& info)
{
    JobGroup wait_handle;
    gpu_submit(&wait_handle, device_handle, info);
    job_wait(&wait_handle);
}

void gpu_present(const DeviceHandle& device_handle, const SwapchainHandle& swapchain_handle)
{
    auto& device = validate_device(device_handle);
    auto& swapchain = device.swapchains[swapchain_handle];

    // ensure the swapchain has acquired its next image before presenting if not already acquired
    get_or_acquire_swapchain_image(&device, &swapchain);

    auto info = VkPresentInfoKHR{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &swapchain.render_semaphore[swapchain.current_image];
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain.handle;
    info.pImageIndices = &swapchain.current_image;
    info.pResults = nullptr;

    device.graphics_queue.present_threadsafe(&info);

    // prepare to acquire next image in the next present
    swapchain.pending_image_acquire = true;
}

void gpu_commit_frame(const DeviceHandle& device_handle)
{
    auto& device = validate_device(device_handle);

    device.current_frame = (device.current_frame + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;
    device.scratch_allocator.reset();
    device.submit_queue_tail.store(0, std::memory_order_release);
}

i32 gpu_get_current_frame(const DeviceHandle& device_handle)
{
    return validate_device(device_handle).current_frame;
}


} // namespace bee