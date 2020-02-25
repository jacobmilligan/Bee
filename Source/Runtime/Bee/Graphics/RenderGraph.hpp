/*
 *  RenderGraph.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/SoA.hpp"
#include "Bee/Core/Functional.hpp"
#include "Bee/Graphics/GPU.hpp"
#include "Bee/Graphics/Command.hpp"

#ifndef BEE_RENDERGRAPH_PASS_MAX_READS
    #define BEE_RENDERGRAPH_PASS_MAX_READS 64
#endif // BEE_RENDERGRAPH_PASS_MAX_READS

#ifndef BEE_RENDERGRAPH_PASS_MAX_SUBPASSES
    #define BEE_RENDERGRAPH_PASS_MAX_SUBPASSES 4
#endif // BEE_RENDERGRAPH_PASS_MAX_SUBPASSES

namespace bee {


enum class RenderGraphResourceType : i32
{
    unknown,
    texture,
    buffer,
    imported_texture,
    imported_buffer
};

enum class RenderGraphAttachmentType : i32
{
    unknown,
    color,
    depth,
    input
};


struct RenderGraphResource
{
    i32                     index { -1 };
    RenderGraphResourceType type { RenderGraphResourceType::unknown };

    RenderGraphResource() = default;

    RenderGraphResource(const i32 resource_index, const RenderGraphResourceType resource_type)
        : index(resource_index),
          type(resource_type)
    {}

    inline constexpr bool is_valid() const
    {
        return index >= 0 && type != RenderGraphResourceType::unknown;
    }

    inline constexpr bool is(const RenderGraphResourceType check_type) const
    {
        return type == check_type;
    }
};

inline constexpr bool operator==(const RenderGraphResource& lhs, const RenderGraphResource& rhs)
{
    return lhs.index == rhs.index && lhs.type == rhs.type;
}

inline constexpr bool operator!=(const RenderGraphResource& lhs, const RenderGraphResource& rhs)
{
    return !(lhs == rhs);
}

class RenderGraph;

class RenderGraphExecuteContext
{
public:
//    CommandBuffer       cmd;

    RenderGraphExecuteContext(RenderGraph* graph, const RenderPassHandle& pass);

    BufferHandle get_buffer(const RenderGraphResource& handle) const;

    TextureHandle get_texture(const RenderGraphResource& handle) const;

    inline const RenderPassHandle& get_pass() const
    {
        return pass_;
    }
private:
    const RenderGraph*  graph_ { nullptr };
    RenderPassHandle    pass_;
};


struct RenderGraphPass
{
    using execute_function_t = Function<void(RenderGraphExecuteContext* ctx), 64>;

    const char*                 name { nullptr };
    execute_function_t          execute;
    RenderPassCreateInfo        info;
    RenderPassHandle            physical_pass;
    i32                         write_count { 0 };
    i32                         read_count { 0 };
    RenderGraphResource         resources_read[BEE_RENDERGRAPH_PASS_MAX_READS];
    RenderGraphResource         attachment_textures[BEE_GPU_MAX_ATTACHMENTS];
    RenderGraphAttachmentType   attachment_types[BEE_GPU_MAX_ATTACHMENTS];
    SubPassDescriptor           subpasses[BEE_RENDERGRAPH_PASS_MAX_SUBPASSES];

    inline void reset(const char* new_name)
    {
        name = new_name;
        new (&execute) execute_function_t();
        write_count = 0;
        read_count = 0;

        info.attachment_count = 0;
        info.attachment_count = 0;
        info.subpass_count = 1;
        info.subpasses = subpasses;
    }
};


class BEE_RUNTIME_API RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, const i32 pass_index);

    RenderGraphResource create_buffer(const char* name, const BufferCreateInfo& create_info);

    RenderGraphResource create_texture(const char* name, const TextureCreateInfo& create_info);

    RenderGraphBuilder& write_color(const RenderGraphResource& texture, const LoadOp load, const StoreOp store);

    RenderGraphBuilder& write_depth(const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store);

    template <typename Fn>
    RenderGraphBuilder& set_execute_function(Fn&& function)
    {
        graph_->set_pass_execute_function(pass_index_, std::forward<Fn>(function));
        return *this;
    }
private:
    RenderGraph*    graph_ { nullptr };
    i32             pass_index_ { -1 };
};


class JobGroup;


class BEE_RUNTIME_API RenderGraph
{
public:
    explicit RenderGraph(const DeviceHandle& device);

    ~RenderGraph();

    RenderGraphBuilder add_pass(const char* name);

    template <typename Fn>
    void set_pass_execute_function(const i32 pass_index, Fn&& function)
    {
        BEE_ASSERT(pass_index <= next_pass_);
        passes_[pass_index].execute = function;
    }

    RenderGraphResource get_or_create_buffer(const char* name, const BufferCreateInfo& create_info);

    RenderGraphResource get_or_create_texture(const char* name, const TextureCreateInfo& create_info);

    BufferHandle get_physical_buffer(const RenderGraphResource& handle) const;

    TextureHandle get_physical_texture(const RenderGraphResource& handle) const;

    void write_resource(const i32 pass_index, const RenderGraphResource& handle);

    void read_resource(const i32 pass_index, const RenderGraphResource& handle);

    bool add_attachment(const i32 pass_index, const RenderGraphResource& texture, const RenderGraphAttachmentType type, const AttachmentDescriptor& desc);

    void execute(JobGroup* wait_handle);
private:
    template <typename ResourceType, typename CreateInfoType>
    struct SimplePool
    {
        DynamicArray<u32>               hashes;
        DynamicArray<const char*>       names;
        DynamicArray<i32>               active_index;
        DynamicArray<ResourceType>      resources;
        DynamicArray<CreateInfoType>    create_infos;

        i32 get_or_create(const char* new_name, const i32 new_active_index, const CreateInfoType& create_info)
        {
            const auto info_hash = get_hash(create_info);
            int index = -1;

            for (auto hash : enumerate(hashes))
            {
                if (hash.value == info_hash && active_index[hash.index] < 0)
                {
                    index = hash.index;
                    break;
                }
            }

            if (index < 0)
            {
                index = hashes.size();
                hashes.push_back(info_hash);
                names.push_back("");
                active_index.push_back(-1);
                resources.emplace_back();
                create_infos.push_back(create_info);
            }

            names[index] = new_name;
            active_index[index] = new_active_index;
            return index;
        }

        bool is_valid(const i32 index)
        {
            return index < hashes.size() && in_use[index];
        }
    };

    struct ActiveResourceList
    {
        i32                                 size { 0 };
        Allocator*                          allocator { nullptr };

        // Data
        FixedArray<i32>                     physical_indices;
        FixedArray<RenderGraphResourceType> types;
        FixedArray<i32>                     reference_counts;
        FixedArray<DynamicArray<i32>>       writer_passes;

        ActiveResourceList() = default;

        explicit ActiveResourceList(Allocator* new_allocator)
            : allocator(new_allocator),
              physical_indices(new_allocator),
              types(new_allocator),
              reference_counts(new_allocator),
              writer_passes(new_allocator)
        {}
    };

    struct TextureResource
    {
        TextureHandle       handle;
        TextureViewHandle   view;
    };

    struct PhysicalPassPool
    {
        DynamicArray<u32>               hashes;
        DynamicArray<RenderPassHandle>  handles;
    };

    // GPU resources
    DeviceHandle                                    device_;
    CommandBatcher                                  command_batcher_;
    FixedArray<CommandAllocator>                    per_worker_command_allocators_;

    // Resources
    SimplePool<BufferHandle, BufferCreateInfo>      buffers_;
    SimplePool<TextureResource, TextureCreateInfo>  textures_;
    ActiveResourceList                              active_list_;

    // Passes
    i32                                             next_pass_ { 0 };
    DynamicArray<RenderGraphPass>                   passes_;
    PhysicalPassPool                                physical_passes_;

    static RenderGraphResource add_active_resource(ActiveResourceList* list, const i32 index, const RenderGraphResourceType& type);

    static RenderPassHandle obtain_physical_pass(const DeviceHandle& device, PhysicalPassPool* pool, const RenderPassCreateInfo& create_info);
};


} // namespace bee