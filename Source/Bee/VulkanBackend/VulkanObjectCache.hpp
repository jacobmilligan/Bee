/*
 *  VulkanCache.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include "Bee/Gpu/Gpu.hpp"

#include <volk.h>


namespace bee {


struct VulkanDevice;

/*
 **************************
 *
 * Pipeline layout cache
 *
 **************************
 */
struct VulkanPipelineLayoutKey
{
    u32                             resource_layout_count { 0 };
    const ResourceLayoutDescriptor* resource_layouts { nullptr };
    u32                             push_constant_range_count { 0 };
    const PushConstantRange*        push_constant_ranges { nullptr };
};

inline bool operator==(const VulkanPipelineLayoutKey& lhs, const VulkanPipelineLayoutKey& rhs)
{
    if (lhs.resource_layout_count != rhs.resource_layout_count || lhs.push_constant_range_count != rhs.push_constant_range_count)
    {
        return false;
    }

    for (u32 i = 0; i < lhs.resource_layout_count; ++i)
    {
        if (lhs.resource_layouts[i] != rhs.resource_layouts[i])
        {
            return false;
        }
    }

    for (u32 i = 0; i < lhs.push_constant_range_count; ++i)
    {
        if (lhs.push_constant_ranges[i] != rhs.push_constant_ranges[i])
        {
            return false;
        }
    }

    return true;
}

template <>
struct Hash<VulkanPipelineLayoutKey>
{
    inline u32 operator()(const VulkanPipelineLayoutKey& key) const
    {
        HashState hash;
        hash.add(key.resource_layout_count);
        for (u32 i = 0; i < key.resource_layout_count; ++i)
        {
            hash.add(key.resource_layouts[i].resources.size);
            hash.add(key.resource_layouts[i].resources.data, sizeof(ResourceDescriptor) * key.resource_layouts[i].resources.size);
        }
        hash.add(key.push_constant_range_count);
        hash.add(key.push_constant_ranges, sizeof(PushConstantRange) * key.push_constant_range_count);
        return hash.end();
    }
};

/*
 **********************
 *
 * Framebuffer cache
 *
 **********************
 */
struct VulkanFramebufferKey
{
    struct FormatKey
    {
        PixelFormat format { PixelFormat::unknown };
        u32         sample_count { 0 };
    };

    u32             width { 0 };
    u32             height { 0 };
    u32             layers { 0 };
    u32             attachment_count { 0 };
    FormatKey       format_keys[BEE_GPU_MAX_ATTACHMENTS];
    VkImageView     attachments[BEE_GPU_MAX_ATTACHMENTS];

    // not hashed - we hash the format keys in its place
    VkRenderPass    compatible_render_pass { VK_NULL_HANDLE };
};

inline bool operator==(const VulkanFramebufferKey& lhs, const VulkanFramebufferKey& rhs)
{
    if (lhs.width != rhs.width
        || lhs.height != rhs.height
        || lhs.layers  != rhs.layers
        || lhs.attachment_count != rhs.attachment_count)
    {
        return false;
    }

    for (u32 i = 0; i < lhs.attachment_count; ++i)
    {
        if (lhs.attachments[i] != rhs.attachments[i])
        {
            return false;
        }

        if (lhs.format_keys[i].format != rhs.format_keys[i].format)
        {
            return false;
        }

        if (lhs.format_keys[i].sample_count != rhs.format_keys[i].sample_count)
        {
            return false;
        }
    }

    return true;
}

template <>
struct Hash<VulkanFramebufferKey>
{
    u32 inline operator()(const VulkanFramebufferKey& key) const
    {
        HashState hash;
        hash.add(key.width);
        hash.add(key.height);
        hash.add(key.layers);
        hash.add(key.attachment_count);
        hash.add(key.format_keys, sizeof(VulkanFramebufferKey::FormatKey) * key.attachment_count);
        hash.add(key.attachments, sizeof(VkImageView) * key.attachment_count);
        return hash.end();
    }
};


/*
 **********************
 *
 * Object cache
 *
 **********************
 */
template <typename KeyType, typename ValueType>
class VulkanPendingCache
{
public:
    using create_func_t = void(*)(VulkanDevice* device, const KeyType& key, ValueType* value);
    using destroy_func_t = void(*)(VulkanDevice* device, ValueType* value);

    void init(VulkanDevice* device, create_func_t on_create, destroy_func_t on_destroy)
    {
        device_ = device;
        on_create_ = on_create;
        on_destroy_ = on_destroy;
        pending_creates_.resize(job_system_worker_count());
    }

    void destroy()
    {
        clear();
    }

    ValueType& get_or_create(const KeyType& key)
    {
        auto* value = shared_cache_.find(key);
        if (value != nullptr)
        {
            return value->value;
        }

        auto& thread = pending_creates_[job_worker_id()];
        const i32 pending_index = find_index(thread.keys, key);

        if (pending_index >= 0)
        {
            return thread.values[pending_index];
        }

        thread.keys.push_back(key);
        thread.values.emplace_back();
        on_create_(device_, key, &thread.values.back());
        return thread.values.back();
    }

    void sync()
    {
        for (auto& queue : pending_creates_)
        {
            for (int i = 0; i < queue.keys.size(); ++i)
            {
                if (shared_cache_.find(queue.keys[i]) == nullptr)
                {
                    shared_cache_.insert(queue.keys[i], queue.values[i]);
                }
                else
                {
                    pending_deletes_[current_frame_].push_back(queue.values[i]);
                }
            }

            queue.keys.clear();
            queue.values.clear();
        }

        current_frame_ = (current_frame_ + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;
        auto& duplicates = pending_deletes_[current_frame_];
        for (auto& duplicate : duplicates)
        {
            on_destroy_(device_, &duplicate);
        }
        duplicates.clear();
    }

    void clear()
    {
        for (auto& queue : pending_creates_)
        {
            for (auto& value : queue.values)
            {
                on_destroy_(device_, &value);
            }

            queue.keys.clear();
            queue.values.clear();
        }

        for (auto& duplicates : pending_deletes_)
        {
            for (auto& duplicate : duplicates)
            {
                on_destroy_(device_, &duplicate);
            }

            duplicates.clear();
        }

        for (auto& keyval : shared_cache_)
        {
            on_destroy_(device_, &keyval.value);
        }

        shared_cache_.clear();
    }

private:
    struct PendingQueue
    {
        DynamicArray<KeyType>   keys;
        DynamicArray<ValueType> values;
    };

    FixedArray<PendingQueue>            pending_creates_;
    DynamicHashMap<KeyType, ValueType>  shared_cache_;
    DynamicArray<ValueType>             pending_deletes_[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    i32                                 current_frame_ { 0 };

    VulkanDevice*                       device_ { nullptr };
    create_func_t                       on_create_ { nullptr };
    destroy_func_t                      on_destroy_ { nullptr };
};


/*
 ********************************************
 *
 * Vulkan cached object create/destroy
 * operations
 *
 ********************************************
 */
void create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key, VkDescriptorSetLayout* layout);

void destroy_descriptor_set_layout(VulkanDevice* device, VkDescriptorSetLayout* layout);

void create_pipeline_layout(VulkanDevice* device, const VulkanPipelineLayoutKey& key, VkPipelineLayout* layout);

void destroy_pipeline_layout(VulkanDevice* device, VkPipelineLayout* layout);

void create_framebuffer(VulkanDevice* device, const VulkanFramebufferKey& key, VkFramebuffer* framebuffer);

void destroy_framebuffer(VulkanDevice* device, VkFramebuffer* framebuffer);



} // namespace bee