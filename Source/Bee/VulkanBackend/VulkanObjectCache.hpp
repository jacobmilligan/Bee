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

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    #include <volk.h>
BEE_POP_WARNING


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
    u32                             push_constant_range_count { 0 };
    const ResourceLayoutDescriptor* resource_layouts { nullptr };
    const PushConstantRange*        push_constant_ranges { nullptr };
};

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
 * Pipeline cache
 *
 **********************
 */
struct VulkanPipelineKey
{
    const PipelineStateDescriptor*  desc;
    u32                             render_pass_hash { 0 };
    u32                             subpass_index { 0 };
    u32                             shader_hashes[ShaderStageIndex::count] { 0 };
    VkRenderPass                    render_pass { VK_NULL_HANDLE };
};

template <>
struct Hash<VulkanPipelineKey>
{
    u32 inline operator()(const VulkanPipelineKey& key) const
    {
        HashState hash;
        hash.add(*key.desc);
        hash.add(key.render_pass_hash);
        hash.add(key.shader_hashes, sizeof(u32) * static_array_length(key.shader_hashes));
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
        thread_local_pending_creates_.resize(job_system_worker_count());
    }

    void destroy()
    {
        clear();
    }

    ValueType& get_or_create(const KeyType& key)
    {
        // Use a generic u32 hash as the key instead of the actual key - the actual keys often have pointers
        // to descriptor structs etc. that might change per frame
        const u32 hash = get_hash(key);
        auto* value = shared_cache_.find(hash);
        if (value != nullptr)
        {
            return value->value;
        }

        auto& thread = thread_local_pending_creates_[job_worker_id()];
        const i32 pending_index = find_index(thread.hashes, hash);

        if (pending_index >= 0)
        {
            return thread.values[pending_index];
        }

        thread.hashes.push_back(hash);
        thread.keys.push_back(key);
        thread.values.emplace_back();
        on_create_(device_, key, &thread.values.back());
        return thread.values.back();
    }

    void sync()
    {
        for (auto& queue : thread_local_pending_creates_)
        {
            for (int i = 0; i < queue.keys.size(); ++i)
            {
                if (shared_cache_.find(queue.hashes[i]) == nullptr)
                {
                    shared_cache_.insert(queue.hashes[i], queue.values[i]);
                }
                else
                {
                    pending_deletes_[current_frame_].push_back(queue.values[i]);
                }
            }

            queue.clear();
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
        for (auto& queue : thread_local_pending_creates_)
        {
            for (auto& value : queue.values)
            {
                on_destroy_(device_, &value);
            }

            queue.clear();
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
        DynamicArray<u32>       hashes;
        DynamicArray<KeyType>   keys;
        DynamicArray<ValueType> values;

        void clear()
        {
            hashes.clear();
            keys.clear();
            values.clear();
        }
    };

    FixedArray<PendingQueue>            thread_local_pending_creates_;
    DynamicHashMap<u32, ValueType>      shared_cache_;
    DynamicArray<ValueType>             pending_deletes_[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    i32                                 current_frame_ { 0 };
    BEE_PAD(4);

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
struct VulkanPipelineState;

void create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key, VkDescriptorSetLayout* layout);

void destroy_descriptor_set_layout(VulkanDevice* device, VkDescriptorSetLayout* layout);

void create_pipeline_layout(VulkanDevice* device, const VulkanPipelineLayoutKey& key, VkPipelineLayout* layout);

void destroy_pipeline_layout(VulkanDevice* device, VkPipelineLayout* layout);

void create_framebuffer(VulkanDevice* device, const VulkanFramebufferKey& key, VkFramebuffer* framebuffer);

void destroy_framebuffer(VulkanDevice* device, VkFramebuffer* framebuffer);

void create_pipeline(VulkanDevice* device, const VulkanPipelineKey& key, VulkanPipelineState* pipeline);

void destroy_pipeline(VulkanDevice* device, VulkanPipelineState* pipeline);



} // namespace bee