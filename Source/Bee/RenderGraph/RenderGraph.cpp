/*
 *  RenderGraph.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/RenderGraph/RenderGraph.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Memory/ChunkAllocator.hpp"


namespace bee {


static constexpr size_t rg_pass_data_capacity = 4096;

struct VirtualBuffer
{
    BufferCreateInfo    create_info;
    BufferHandle        handle;
};

struct VirtualTexture
{
    TextureCreateInfo   create_info;
    TextureHandle       handle;
    TextureViewHandle   view_handle;
};

struct VirtualBackBuffer
{
    SwapchainHandle     swapchain;
    TextureHandle       drawable;
    TextureViewHandle   drawable_view;
};

struct VirtualResource
{
    RenderGraphResource handle;

    union
    {
        VirtualBuffer      buffer;
        VirtualTexture     texture;
        VirtualBackBuffer  backbuffer;
    };

    u32                             hash { 0 };
    const char*                     name { nullptr };
    i32                             refcount { 0 };
    i32                             pool_index { -1 };
    DynamicArray<RenderGraphPass*>  writer_passes;

    VirtualResource()
    {
        const auto size = math::max(
            math::max(sizeof(VirtualBuffer), sizeof(VirtualTexture)),
            sizeof(VirtualBackBuffer)
        );
        memset(&buffer, 0, size);
    }
};

struct PooledPass
{
    u32                 hash { 0 };
    RenderPassHandle    handle;
};

struct PooledResource
{
    struct Texture
    {
        TextureHandle       handle;
        TextureViewHandle   view_handle;
    };

    struct Buffer
    {
        BufferHandle    handle;
        size_t          size { 0 };
    };

    u32                     hash { 0 };
    RenderGraphResourceType type { RenderGraphResourceType::imported_buffer };
    GpuResourceState        state { GpuResourceState::undefined };

    union
    {
        Buffer  buffer;
        Texture texture;
    };
};

struct RenderGraphPass // NOLINT
{
    RenderGraph*                        graph { nullptr };
    RenderPassHandle                    handle;
    CommandBuffer*                      cmdbuf { nullptr };

    i32                                 write_count { 0 };
    DynamicArray<RenderGraphResource>   reads;

    i32                                 attachment_count { 0 };
    AttachmentDescriptor                attachments[BEE_GPU_MAX_ATTACHMENTS];
    RenderGraphResource                 attachment_rg_resources[BEE_GPU_MAX_ATTACHMENTS];
    TextureViewHandle                   attachment_textures[BEE_GPU_MAX_ATTACHMENTS];

    RenderGraphPassDesc                 desc;
    u8                                  external_data[rg_pass_data_capacity];
    u8                                  data[rg_pass_data_capacity];
    bool                                has_transitioned_resources { false };
    bool                                enabled { true };
};

struct RenderGraph
{
    RenderGraph*                        next { nullptr };
    RenderGraph*                        prev { nullptr };

    GpuBackend*                         backend { nullptr };
    DeviceHandle                        device;
    JobGroup                            wait_handle;

    DynamicArray<VirtualResource>       virtual_resources;
    DynamicArray<RenderGraphPass*>      virtual_passes;

    DynamicArray<VirtualResource*>      frontier;
    DynamicArray<RenderGraphPass*>      execute_order;
    DynamicArray<VirtualResource*>      executed_resources;

    RenderPassCreateInfo                tmp_pass_info;
    DynamicArray<PooledResource>        resource_pool;
    DynamicArray<PooledPass>            pass_pool;
    FixedArray<CommandBuffer*>          executed_cmd_buffers;
};

struct RenderGraphModuleData
{
    RenderGraph*    first_graph { nullptr };
    RenderGraph*    last_graph { nullptr };
    ChunkAllocator  pass_allocator;

    RenderGraphModuleData()
        : pass_allocator(sizeof(RenderGraphPass) * 32, 64, 0)
    {}
};


static RenderGraphModule        g_module;
static RenderGraphBuilderModule g_builder_module;
static RenderGraphStorage       g_storage;
static RenderGraphModuleData*   g_data { nullptr };

/*
 ********************************************
 *
 * RenderGraphStorage - implementation
 *
 ********************************************
 */
BufferHandle get_buffer(RenderGraphPass* pass, const RenderGraphResource& handle)
{
    if (BEE_FAIL(handle.index() < pass->graph->virtual_resources.size()))
    {
        return BufferHandle{};
    }

    if (BEE_FAIL_F(handle == RenderGraphResourceType::buffer || handle == RenderGraphResourceType::imported_buffer, "Invalid resource handle"))
    {
        return BufferHandle{};
    }

    return pass->graph->virtual_resources[sign_cast<i32>(handle.index())].buffer.handle;
}

TextureHandle get_texture(RenderGraphPass* pass, const RenderGraphResource& handle)
{
    if (BEE_FAIL(handle.index() < pass->graph->virtual_resources.size()))
    {
        return TextureHandle{};
    }

    if (BEE_FAIL_F(handle == RenderGraphResourceType::texture || handle == RenderGraphResourceType::imported_texture, "Invalid resource handle"))
    {
        return TextureHandle{};
    }

    return pass->graph->virtual_resources[sign_cast<i32>(handle.index())].texture.handle;
}

u32 get_attachments(RenderGraphPass* pass, const TextureViewHandle** dst)
{
    if (pass->attachment_count <= 0)
    {
        return 0;
    }

    *dst = pass->attachment_textures;
    return sign_cast<u32>(pass->attachment_count);
}

RenderPassHandle get_gpu_pass(RenderGraphPass* pass)
{
    return pass->handle;
}

Extent get_backbuffer_size(RenderGraphPass* pass, const RenderGraphResource& handle)
{
    const auto index = sign_cast<i32>(handle.index());
    if (BEE_FAIL_F(index < pass->graph->virtual_resources.size(), "Invalid resource handle"))
    {
        return Extent{};
    }

    if (BEE_FAIL_F(pass->graph->virtual_resources[index].handle == RenderGraphResourceType::backbuffer, "Resource handle is not a backbuffer"))
    {
        return Extent{};
    }

    auto* backend = pass->graph->backend;
    return backend->get_swapchain_extent(pass->graph->device, pass->graph->virtual_resources[index].backbuffer.swapchain);
}

RenderRect get_backbuffer_rect(RenderGraphPass* pass, const RenderGraphResource& handle)
{
    const auto index = sign_cast<i32>(handle.index());
    if (BEE_FAIL_F(index < pass->graph->virtual_resources.size(), "Invalid resource handle"))
    {
        return RenderRect{};
    }

    if (BEE_FAIL_F(pass->graph->virtual_resources[index].handle == RenderGraphResourceType::backbuffer, "Resource handle is not a backbuffer"))
    {
        return RenderRect{};
    }

    auto* backend = pass->graph->backend;
    const auto extent = backend->get_swapchain_extent(pass->graph->device, pass->graph->virtual_resources[index].backbuffer.swapchain);
    return RenderRect(0, 0, extent.width, extent.height);
}

DeviceHandle get_device(RenderGraphPass* pass)
{
    return pass->graph->device;
}


/*
 ********************************************
 *
 * RenderGraphBuilder - implementation
 *
 ********************************************
 */
template <>
struct Hash<BufferHandle>
{
    inline u32 operator()(const BufferHandle& key)
    {
        return get_hash(key.id);
    }
};

template <>
struct Hash<TextureHandle>
{
    inline u32 operator()(const TextureHandle& key)
    {
        return get_hash(key.id);
    }
};

template <typename T>
inline RenderGraphResource add_resource(RenderGraph* graph, const char* name, const RenderGraphResourceType type, const T& create_info_or_handle)
{
    const auto index = graph->virtual_resources.size();
    graph->virtual_resources.emplace_back();

    auto& resource = graph->virtual_resources.back();
    resource.hash = get_hash(create_info_or_handle);
    resource.name = name;
    resource.handle = RenderGraphResource(index, underlying_t(type));
    resource.writer_passes.clear();

    switch(type)
    {
        case RenderGraphResourceType::buffer:
        {
            memcpy(&resource.buffer.create_info, &create_info_or_handle, sizeof(T));
            break;
        }
        case RenderGraphResourceType::imported_buffer:
        {
            memcpy(&resource.buffer.handle, &create_info_or_handle, sizeof(T));
            break;
        }
        case RenderGraphResourceType::texture:
        {
            memcpy(&resource.texture.create_info, &create_info_or_handle, sizeof(T));
            break;
        }
        case RenderGraphResourceType::imported_texture:
        {
            memcpy(&resource.texture.handle, &create_info_or_handle, sizeof(T));
            break;
        }
        case RenderGraphResourceType::backbuffer:
        {
            memcpy(&resource.backbuffer, &create_info_or_handle, sizeof(T));
            break;
        }
        default:
        {
            BEE_UNREACHABLE("Not implemented");
        }
    }

    return resource.handle;
}

RenderGraphResource create_buffer(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info)
{
    return add_resource(pass->graph, name, RenderGraphResourceType::buffer, create_info);
}

RenderGraphResource create_texture(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info)
{
    return add_resource(pass->graph, name, RenderGraphResourceType::texture, create_info);
}

RenderGraphResource import_buffer(RenderGraphPass* pass, const char* name, const BufferHandle& buffer)
{
    return add_resource(pass->graph, name, RenderGraphResourceType::imported_buffer, buffer);
}

RenderGraphResource import_texture(RenderGraphPass* pass, const char* name, const TextureHandle& texture)
{
    return add_resource(pass->graph, name, RenderGraphResourceType::imported_texture, texture);
}

RenderGraphResource import_backbuffer(RenderGraphPass* pass, const char* name, const SwapchainHandle& swapchain)
{
    return add_resource(pass->graph, name, RenderGraphResourceType::backbuffer, swapchain);
}

void write_resource(RenderGraphPass* pass, const RenderGraphResource& resource)
{
    BEE_ASSERT(pass != nullptr);

    auto& pooled = pass->graph->virtual_resources[sign_cast<i32>(resource.index())];

    ++pass->write_count;
    pooled.writer_passes.push_back(pass);
}

void rg_read_resource(RenderGraphPass* pass, const RenderGraphResource& resource)
{
    BEE_ASSERT(pass != nullptr);

    auto& pooled = pass->graph->virtual_resources[sign_cast<i32>(resource.index())];
    ++pooled.refcount;
    pass->reads.push_back(resource);
}

bool add_attachment(RenderGraphPass* pass, const RenderGraphResource& texture, const AttachmentDescriptor& desc)
{
    BEE_ASSERT(pass != nullptr);

    // if the attachment texture is already added we need to replace the attachment stored in the pass with the new one
    for (int i = 0; i < pass->attachment_count; ++i)
    {
        if (pass->attachment_rg_resources[i] == texture)
        {
            pass->attachments[i] = desc;
            return true;
        }
    }

    // adding a new attachment texture
    if (pass->attachment_count >= BEE_GPU_MAX_ATTACHMENTS)
    {
        log_error("Cannot add more than BEE_GPU_MAX_ATTACHMENTS (%u) attachments to the same RenderGraph pass", BEE_GPU_MAX_ATTACHMENTS);
        return false;
    }

    pass->attachments[pass->attachment_count] = desc;
    pass->attachment_rg_resources[pass->attachment_count] = texture;
    ++pass->attachment_count;
    return true;
}

void write_color(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store, const u32 samples)
{
    BEE_ASSERT(texture != RenderGraphResourceType::buffer && texture != RenderGraphResourceType::imported_buffer);

    AttachmentDescriptor desc{};
    desc.type = texture == RenderGraphResourceType::backbuffer ? AttachmentType::present : AttachmentType::color;
    desc.format = PixelFormat::unknown; // we'll get the color later from the texture format
    desc.load_op = load;
    desc.store_op = store;
    desc.samples = samples;

    if (add_attachment(pass, texture, desc))
    {
        write_resource(pass, texture);
    }
}

void write_depth(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store)
{
    BEE_ASSERT(texture != RenderGraphResourceType::buffer && texture != RenderGraphResourceType::imported_buffer);
    BEE_ASSERT_F(is_depth_format(depth_format), "depth_format is not a valid depth-stencil pixel format");

    AttachmentDescriptor desc{};
    desc.type = AttachmentType::depth_stencil;
    desc.format = depth_format;
    desc.load_op = load;
    desc.store_op = store;
    desc.samples = 1;

    if (add_attachment(pass, texture, desc))
    {
        write_resource(pass, texture);
    }
}

RenderGraphPass* add_static_pass(RenderGraph* graph, const RenderGraphPassDesc& desc)
{
    if (BEE_FAIL_F(desc.external_data_size <= rg_pass_data_capacity, "Failed to add RenderGraph pass: data_size was >= rg_pass_data_capacity (%llu > %llu)", desc.pass_data_size, rg_pass_data_capacity))
    {
        return nullptr;
    }

    if (BEE_FAIL_F(desc.pass_data_size <= rg_pass_data_capacity, "Failed to add RenderGraph pass: data_size was >= rg_pass_data_capacity (%llu > %llu)", desc.pass_data_size, rg_pass_data_capacity))
    {
        return nullptr;
    }

    BEE_ASSERT(desc.setup != nullptr);
    BEE_ASSERT(desc.execute != nullptr);

    auto* pass = BEE_NEW(g_data->pass_allocator, RenderGraphPass);
    graph->virtual_passes.push_back(pass);

    pass->has_transitioned_resources = false;
    pass->attachment_count = 0;
    pass->reads.clear();
    pass->write_count = 0;
    pass->graph = graph;
    pass->handle = RenderPassHandle{};
    memcpy(&pass->desc, &desc, sizeof(RenderGraphPassDesc));

    if (desc.external_data_size > 0)
    {
        // copy the external data
        memcpy(pass->external_data, desc.external_data, desc.external_data_size);
    }

    if (desc.pass_data_size > 0)
    {
        // zero the pass data buffer
        memset(pass->data, 0, desc.pass_data_size);
    }

    if (desc.init != nullptr)
    {
        desc.init(pass->external_data, pass->data);
    }

    return pass;
}

void remove_pass(RenderGraphPass* pass)
{
    auto* graph = pass->graph;
    const int index = find_index(graph->virtual_passes, pass);
    if (BEE_FAIL_F(index >= 0, "RenderGraphPass is invalid"))
    {
        return;
    }

    if (pass->desc.destroy != nullptr)
    {
        pass->desc.destroy(pass->external_data, pass->data);
    }

    graph->virtual_passes.erase(index);
    BEE_DELETE(g_data->pass_allocator, pass);
}

void disable_pass(RenderGraphPass* pass)
{
    pass->enabled = false;
}

void import_render_pass(RenderGraphPass* pass, const RenderPassHandle& handle, const i32 attachment_count, const AttachmentDescriptor* attachments, const RenderGraphResource* resources)
{
    pass->handle = handle;

    bool has_depth_stencil = false;

    for (int i = 0; i < attachment_count; ++i)
    {
        BEE_ASSERT_F(!has_depth_stencil || !is_depth_stencil_format(attachments[i].format), "Multiple depth stencil attachments specified in RenderPass");
        BEE_ASSERT(resources[i] != RenderGraphResourceType::imported_buffer && resources[i] != RenderGraphResourceType::buffer);

        if (add_attachment(pass, resources[i], attachments[i]))
        {
            write_resource(pass, resources[i]);
            if (is_depth_stencil_format(attachments[i].format))
            {
                has_depth_stencil = true;
            }
        }
    }
}

/*
 ********************************************
 *
 * RenderGraphModule - implementation
 *
 ********************************************
 */
RenderGraph* create_graph(GpuBackend* backend, const DeviceHandle device)
{
    auto* graph = BEE_NEW(system_allocator(), RenderGraph);
    graph->next = nullptr;
    graph->prev = g_data->last_graph;
    graph->backend = backend;
    graph->device = device;

    if (g_data->first_graph == nullptr)
    {
        g_data->first_graph = g_data->last_graph = graph;
    }
    else
    {
        g_data->last_graph->next = graph;
        g_data->last_graph = graph;
    }

    graph->executed_cmd_buffers.resize(job_system_worker_count());

    return graph;
}

void destroy_graph(RenderGraph* graph)
{
    auto* backend = graph->backend;
    backend->submissions_wait(graph->device);

    // cleanup pooled resources
    for (auto& resource : graph->resource_pool)
    {
        switch (resource.type)
        {
            case RenderGraphResourceType::buffer:
            {
                backend->destroy_buffer(graph->device, resource.buffer.handle);
                break;
            }
            case RenderGraphResourceType::texture:
            {
                backend->destroy_texture(graph->device, resource.texture.handle);
                backend->destroy_texture_view(graph->device, resource.texture.view_handle);
                break;
            }
            case RenderGraphResourceType::imported_buffer:
            case RenderGraphResourceType::imported_texture:
            case RenderGraphResourceType::backbuffer:
            {
                break;
            }
        }
    }

    // destroy pooled GPU passes
    for (auto& pass : graph->pass_pool)
    {
        backend->destroy_render_pass(graph->device, pass.handle);
    }

    // destroy virtual API passes
    for (auto* pass : graph->virtual_passes)
    {
        BEE_DELETE(g_data->pass_allocator, pass);
    }

    graph->virtual_passes.clear();

    // unlink before deleting
    if (graph->prev != nullptr)
    {
        graph->prev->next = graph->next;
    }

    if (graph->next != nullptr)
    {
        graph->next->prev = graph->prev;
    }

    if (graph == g_data->first_graph)
    {
        g_data->first_graph = graph->next;
    }

    if (graph == g_data->last_graph)
    {
        g_data->last_graph = graph->prev;
    }

    BEE_DELETE(system_allocator(), graph);
}

static void resolve_resource(RenderGraph* graph, VirtualResource* src)
{
    auto* backend = graph->backend;

    // imported resources already have a GPU handle
    if (src->handle == RenderGraphResourceType::imported_buffer || src->handle == RenderGraphResourceType::imported_texture)
    {
        return;
    }

    // This is probably the latest point we can acquire the swapchains drawables safely, i.e. we need to acquire
    // drawables before executing command buffers because the swapchain may be recreated here
    if (src->handle == RenderGraphResourceType::backbuffer)
    {
        src->backbuffer.drawable = backend->acquire_swapchain_texture(graph->device, src->backbuffer.swapchain);
        src->backbuffer.drawable_view = backend->get_swapchain_texture_view(graph->device, src->backbuffer.swapchain);
        return;
    }

    const auto index = find_index_if(graph->resource_pool, [&](const PooledResource& r)
    {
        return r.hash == src->hash && r.type == src->handle;
    });

    PooledResource* resource = nullptr;

    if (index >= 0)
    {
        resource = &graph->resource_pool[index];
    }
    else
    {
        // create a new pooled resource
        graph->resource_pool.push_back_no_construct();

        resource = &graph->resource_pool.back();
        resource->hash = src->hash;
        resource->type = static_cast<RenderGraphResourceType>(src->handle.type());

        if (resource->type == RenderGraphResourceType::buffer)
        {
            resource->buffer.handle = backend->create_buffer(graph->device, src->buffer.create_info);
            resource->buffer.size = src->buffer.create_info.size;
        }
        else
        {
            auto& texture_info = src->texture.create_info;

            TextureViewCreateInfo view_info{};
            view_info.texture = backend->create_texture(graph->device, src->texture.create_info);
            view_info.type = texture_info.type;
            view_info.format = texture_info.format;
            view_info.mip_level_offset = 0;
            view_info.mip_level_count = texture_info.mip_count;
            view_info.array_element_offset = 0;
            view_info.array_element_count = texture_info.array_element_count;
            view_info.debug_name = texture_info.debug_name;

            resource->texture.handle = view_info.texture;
            resource->texture.view_handle = backend->create_texture_view(graph->device, view_info);
        }
    }

    if (resource->type == RenderGraphResourceType::buffer)
    {
        src->buffer.handle = resource->buffer.handle;
    }
    else
    {
        src->texture.handle = resource->texture.handle;
        src->texture.view_handle = resource->texture.view_handle;
    }

    src->pool_index = index;
}

static void resolve_pass(RenderGraph* graph, RenderGraphPass* pass)
{
    if (pass->handle.is_valid())
    {
        return;
    }

    auto* backend = graph->backend;

    SubPassDescriptor subpass{};

    auto& pass_info = graph->tmp_pass_info;
    pass_info.attachments.size = sign_cast<u32>(pass->attachment_count);
    pass_info.subpass_count = 1;
    pass_info.subpasses = &subpass;

    for (int i = 0; i < pass->attachment_count; ++i)
    {
        pass_info.attachments[i] = pass->attachments[i];

        const i32 per_frame_index = sign_cast<i32>(pass->attachment_rg_resources[i].index());

        BEE_ASSERT(
            graph->virtual_resources[per_frame_index].handle != RenderGraphResourceType::buffer
            && graph->virtual_resources[per_frame_index].handle != RenderGraphResourceType::imported_buffer
        );

        pass->attachment_textures[i] = graph->virtual_resources[per_frame_index].texture.view_handle;

        BEE_ASSERT(pass->attachment_textures[i].is_valid());

        switch (pass->attachments[i].type)
        {
            case AttachmentType::present:
            case AttachmentType::color:
            {
                ++subpass.color_attachments.size;
                subpass.color_attachments[subpass.color_attachments.size - 1] = sign_cast<u32>(i);

                // Resolve the pixel format for the color attachment from the texture
                auto& resource = graph->virtual_resources[sign_cast<i32>(pass->attachment_rg_resources[i].index())];

                // use the GPU backend to get the format instead of the create_info because this may be an imported texture
                if (resource.handle == RenderGraphResourceType::backbuffer)
                {
                    pass_info.attachments[i].format = backend->get_swapchain_texture_format(graph->device, resource.backbuffer.swapchain);
                }
                else
                {
                    pass_info.attachments[i].format = backend->get_texture_format(graph->device, resource.texture.handle);
                }
                break;
            }
            case AttachmentType::depth_stencil:
            {
                subpass.depth_stencil = sign_cast<u32>(i);
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Invalid attachment type");
            }
        }
    }

    const auto hash = get_hash(pass_info);
    auto index = find_index_if(graph->pass_pool, [&](const PooledPass& p)
    {
        return p.hash == hash;
    });

    if (index < 0)
    {
        // No matching pass was found in the pool so create a new one
        const auto new_pass = backend->create_render_pass(graph->device, pass_info);
        BEE_ASSERT(new_pass.is_valid());
        graph->pass_pool.emplace_back();
        graph->pass_pool.back().hash = hash;
        graph->pass_pool.back().handle = new_pass;

        index = graph->pass_pool.size() - 1;
    }

    pass->handle = graph->pass_pool[index].handle;
}

void setup(RenderGraph* graph)
{
    for (auto* pass : graph->virtual_passes)
    {
        pass->desc.setup(pass, &g_builder_module, pass->external_data, pass->data);
    }
}

static void execute_pass_job(RenderGraphPass* pass)
{
    auto* backend = pass->graph->backend;
    auto* cmd = backend->get_command_backend();
    auto* cmdbuf = backend->allocate_command_buffer(pass->graph->device, QueueType::all);
    auto* transitions = BEE_ALLOCA_ARRAY(GpuTransition, pass->attachment_count);

    cmd->begin(cmdbuf, CommandBufferUsage::submit_once);

    for (int i = 0; i < pass->attachment_count; ++i)
    {
        const auto type = static_cast<RenderGraphResourceType>(pass->attachment_rg_resources[i].type());
        const auto index = sign_cast<i32>(pass->attachment_rg_resources[i].index());

        auto& transition = transitions[i];
        PooledResource* resource = nullptr;

        if (type != RenderGraphResourceType::backbuffer)
        {
            BEE_ASSERT(pass->graph->virtual_resources[index].pool_index >= 0);
            transition.old_state = resource->state;
            resource = &pass->graph->resource_pool[pass->graph->virtual_resources[index].pool_index];
            BEE_ASSERT(resource->type == type);
        }

        switch (type)
        {
            case RenderGraphResourceType::imported_buffer:
            case RenderGraphResourceType::buffer:
            {
                transition.new_state = GpuResourceState::uniform_buffer; // TODO(Jacob): THIS IS WRONG - we could have the buffer in any number of states
                transition.barrier.buffer.handle = resource->buffer.handle;
                transition.barrier.buffer.offset = 0;
                transition.barrier.buffer.size = resource->buffer.size;
                transition.barrier_type = GpuBarrierType::buffer;
                break;
            }
            case RenderGraphResourceType::imported_texture:
            case RenderGraphResourceType::texture:
            {
                if (pass->attachments[i].type == AttachmentType::depth_stencil)
                {
                    transition.new_state = GpuResourceState::depth_write; // TODO(Jacob): thid should allow for depth reads as well
                }
                else
                {
                    transition.new_state = GpuResourceState::color_attachment;
                }

                transition.barrier.texture = resource->texture.handle;
                transition.barrier_type = GpuBarrierType::texture;
                break;
            }
            case RenderGraphResourceType::backbuffer:
            {
                const auto backbuffer = pass->graph->virtual_resources[index].backbuffer;
                pass->attachment_textures[i] = backbuffer.drawable_view;

                transition.old_state = GpuResourceState::undefined;
                transition.new_state = GpuResourceState::present;
                transition.barrier_type = GpuBarrierType::texture;
                transition.barrier.texture = backbuffer.drawable;
                break;
            }
        }
    }

    cmd->transition_resources(cmdbuf, sign_cast<u32>(pass->attachment_count), transitions);
    pass->desc.execute(pass, &g_storage, cmd, cmdbuf, pass->external_data, pass->data);

    if (cmd->get_state(cmdbuf) == CommandBufferState::recording)
    {
        cmd->end(cmdbuf);
    }

    pass->cmdbuf = cmdbuf;
}

void execute(RenderGraph* graph)
{
    /*
     * TODO(Jacob):
     * validate:
     *  - for all passes
     *      - check inputs and outputs have same size
     *      - check all buffer/texture/blit inputs and outputs have same usage and size
     *      - check depth stencil input/output dimensions match
     */

    auto& frontier = graph->frontier;
    frontier.clear();
    graph->executed_resources.clear();

    for (auto& resource : graph->virtual_resources)
    {
        if (resource.refcount <= 0)
        {
            frontier.push_back(&resource);
            graph->executed_resources.push_back(&resource);
        }
    }

    /*
     * Resolve dependencies in the graph starting with all leaf nodes and working backwards - essentially
     * a flood-fill algorithm that will ensure redundant resources and passes aren't included in rendered graph
     *
     * let resource ref_count(0) = leaf node
     * let resource_build_stack be a stack containing all leaf nodes
     * let built_resources = all resources linearly ordered in dependency order with redundant resources culled
     *
     * - while there are leaf nodes in resource_build_stack
     *  - pop a leaf node L off the stack and push into build_resources
     *  - for all passes P that have written to L:
     *      - decrement P.ref_count of P
     *      - if P.ref_count > 0 continue
     *      - else for all resources R read by P:
     *          - decrement R.ref_count
     *          - if R.ref_count > 0 continue
     *          - else R is a leaf node so push R onto resource_build_stack
     *
     * Final result should be a linear array of all resources from bottom->top in order of depth
     * with minimal overlap and should also cull all resources and passes not used as their ref_count
     * will be greater than 0
     */

    while (!frontier.empty())
    {
        const auto* resource = frontier.back();
        frontier.pop_back();

        for (auto* pass : resource->writer_passes)
        {
            const auto pass_write_count = --pass->write_count;

            if (pass_write_count > 0)
            {
                // the pass still has resource dependencies
                continue;
            }

            // the pass has no write dependencies - check all the resources it reads from and then add to the final execute list
            for (const auto& dependency : pass->reads)
            {
                const int resource_refcount = --graph->virtual_resources[sign_cast<i32>(dependency.index())].refcount;

                if (resource_refcount > 0)
                {
                    // the resource still has dependent passes
                    continue;
                }

                // We've reached a leaf resource - so add to the frontier and mark as actually used
                frontier.push_back(&graph->virtual_resources[sign_cast<i32>(dependency.index())]);
                graph->executed_resources.push_back(frontier.back());
            }

            if (pass->enabled)
            {
                // add to execute order if enabled - we've found a leaf pass
                graph->execute_order.push_back(pass);
            }
        }
    }

    for (auto* pass: graph->virtual_passes)
    {
        pass->enabled = true;
    }

    BEE_ASSERT(graph->execute_order.size() <= graph->virtual_passes.size());

    // Execute order is now last pass->first - reverse to get it in order of first pass->last
    std::reverse(graph->execute_order.begin(), graph->execute_order.end());

    // Resolve all the resources and passes to their physical passes
    for (auto* resource : graph->executed_resources)
    {
        resolve_resource(graph, resource);
    }

    // resolve all the passes
    for (auto* pass : graph->execute_order)
    {
        resolve_pass(graph, pass);
    }

    graph->executed_cmd_buffers.clear();

    // kick jobs for each pass
    for (auto* pass : graph->execute_order)
    {
        auto* job = create_job(execute_pass_job, pass);
        job_schedule(&graph->wait_handle, job);
    }

    job_wait(&graph->wait_handle);

    for (auto* pass : graph->execute_order)
    {
        if (pass->cmdbuf != nullptr)
        {
            graph->executed_cmd_buffers.push_back(pass->cmdbuf);
        }
        pass->cmdbuf = nullptr;
    }

    if (!graph->executed_cmd_buffers.empty())
    {
        auto* backend = graph->backend;

        SubmitInfo submit_info{};
        submit_info.command_buffer_count = sign_cast<u32>(graph->executed_cmd_buffers.size());
        submit_info.command_buffers = graph->executed_cmd_buffers.data();
        backend->submit(graph->device, submit_info);

        for (auto& resource : graph->virtual_resources)
        {
            if (resource.handle == RenderGraphResourceType::backbuffer)
            {
                backend->present(graph->device, resource.backbuffer.swapchain);
            }
        }
    }

    // reset the graph
    // TODO(Jacob): temp writer/reader array memory - this is malloc'ing each frame atm
    graph->virtual_resources.clear();
    graph->execute_order.clear();
}


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    if (!loader->require_plugin("Bee.Gpu", bee::PluginVersion{0, 0, 0}))
    {
        return;
    }

    bee::g_data = loader->get_static<bee::RenderGraphModuleData>("Bee.RenderGraphModule");

    // RenderGraphStorage
    bee::g_storage.get_buffer = bee::get_buffer;
    bee::g_storage.get_texture = bee::get_texture;
    bee::g_storage.get_attachments = bee::get_attachments;
    bee::g_storage.get_gpu_pass = bee::get_gpu_pass;
    bee::g_storage.get_backbuffer_size = bee::get_backbuffer_size;
    bee::g_storage.get_backbuffer_rect = bee::get_backbuffer_rect;
    bee::g_storage.get_device = bee::get_device;

    // RenderGraphBuilderModule
    bee::g_builder_module.disable_pass = bee::disable_pass;
    bee::g_builder_module.import_render_pass = bee::import_render_pass;
    bee::g_builder_module.create_buffer = bee::create_buffer;
    bee::g_builder_module.create_texture = bee::create_texture;
    bee::g_builder_module.import_buffer = bee::import_buffer;
    bee::g_builder_module.import_texture = bee::import_texture;
    bee::g_builder_module.import_backbuffer = bee::import_backbuffer;
    bee::g_builder_module.write_color = bee::write_color;
    bee::g_builder_module.write_depth = bee::write_depth;

    // RenderGraphModule
    bee::g_module.create_graph = bee::create_graph;
    bee::g_module.destroy_graph = bee::destroy_graph;
    bee::g_module.add_static_pass = bee::add_static_pass;
    bee::g_module.remove_pass = bee::remove_pass;
    bee::g_module.setup = bee::setup;
    bee::g_module.execute = bee::execute;

    loader->set_module(BEE_RENDER_GRAPH_MODULE, &bee::g_module, state);
    loader->set_module(BEE_RENDER_GRAPH_BUILDER_MODULE, &bee::g_builder_module, state);
}

BEE_PLUGIN_VERSION(0, 0, 0)