/*
 *  Renderer.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include <algorithm>


namespace bee {


static constexpr i32    max_swapchains = 32;
static constexpr i32    rg_pass_max_ops = 128;
static constexpr i32    rg_max_passes = 1024;
static constexpr i32    rg_max_resources = 4096;
static constexpr i32    rg_max_cmd = 64;
static constexpr size_t rg_args_capacity = 1024;


struct RegisteredSwapchain
{
    SwapchainKind   kind { SwapchainKind::secondary };
    i32             id { 0 };
    u32             hash { 0 };
    const char*     name { nullptr };
    SwapchainHandle handle;
};

struct PerFrameBuffer
{
    BufferCreateInfo    create_info;
    BufferHandle        handle;
};

struct PerFrameTexture
{
    TextureCreateInfo   create_info;
    TextureHandle       handle;
    TextureViewHandle   view_handle;
    SwapchainHandle     swapchain;
};

struct RenderGraphPass
{
    RenderGraph*            graph { nullptr };
    RenderPassHandle        handle;

    i32                     write_count { 0 };
    i32                     read_count { 0 };
    RenderGraphResource     reads[rg_pass_max_ops];

    i32                     attachment_count { 0 };
    AttachmentDescriptor    attachments[BEE_GPU_MAX_ATTACHMENTS];
    RenderGraphResource     attachment_textures[BEE_GPU_MAX_ATTACHMENTS];

    bool                    has_execute;
    render_graph_execute_t  execute_fn;
    u8                      execute_args[rg_args_capacity];
};

struct PerFrameResource
{
    RenderGraphResource handle;

    union
    {
        PerFrameBuffer    buffer;
        PerFrameTexture   texture;
    };

    u32                 hash { 0 };
    const char*         name { nullptr };
    i32                 refcount {0 };
    i32                 writer_pass_count { 0 };
    RenderGraphPass*    writer_passes[rg_pass_max_ops] { nullptr };

    PerFrameResource()
    {
        memset(&buffer, 0, math::max(sizeof(PerFrameBuffer), sizeof(PerFrameTexture)));
    }
};

struct PooledResource
{
    struct Texture
    {
        TextureHandle       handle;
        TextureViewHandle   view_handle;
    };

    u32                     hash { 0 };
    RenderGraphResourceType type { RenderGraphResourceType::imported_buffer };

    union
    {
        BufferHandle    buffer;
        Texture         texture;
    };
};

struct PooledPass
{
    u32                 hash { 0 };
    RenderPassHandle    handle;
};

struct RenderGraphThreadData
{
    CommandPoolHandle   cmd_pool;
    CommandBuffer       cmd_buffers[rg_max_cmd];
    i32                 cmd_count { 0 };
};

struct RenderGraph
{
    Allocator*                          allocator { nullptr };
    RenderGraph*                        next { nullptr };
    RenderGraph*                        prev { nullptr };

    i32                                 frame_pass_count { 0 };
    i32                                 frame_resource_count { 0 };
    PerFrameResource                    frame_resources[rg_max_resources];
    RenderGraphPass                     frame_passes[rg_max_passes];

    i32                                 execute_count { 0 };
    RenderGraphPass*                    execute_order[rg_max_passes] { nullptr };

    RenderPassCreateInfo                tmp_pass_info;
    DynamicArray<PooledResource>        resource_pool;
    DynamicArray<PooledPass>            pass_pool;

    FenceHandle                         fences[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    FixedArray<RenderGraphThreadData>   thread_data[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
};

struct Renderer
{
    DeviceHandle                device;
    DynamicArray<RenderStage*>  stages;
    i32                         swapchain_count { 0 };
    RegisteredSwapchain         swapchains[max_swapchains];
    RenderGraph*                default_graph { nullptr };
    RenderGraph*                first_graph { nullptr };
    RenderGraph*                last_graph { nullptr };
};

static Renderer* g_renderer { nullptr };

/*
 ********************************************
 *
 * Render graph API - graph level functions
 *
 ********************************************
 */
static RenderGraphBuilderModule     g_builder{};
static RenderGraphStorage           g_storage{};

RenderGraph* rg_create_render_graph(Allocator* allocator)
{
    auto* graph = BEE_NEW(allocator, RenderGraph);
    graph->next = nullptr;
    graph->prev = g_renderer->last_graph;
    graph->allocator = allocator;
    graph->frame_pass_count = 0;
    graph->frame_resource_count = 0;

    if (g_renderer->first_graph == nullptr)
    {
        g_renderer->first_graph = g_renderer->last_graph = graph;
    }
    else
    {
        g_renderer->last_graph->next = graph;
        g_renderer->last_graph = graph;
    }

    CommandPoolCreateInfo cmd_pool_info{};
    cmd_pool_info.used_queues_hint = QueueType::all;
    cmd_pool_info.pool_hint = CommandPoolHint::transient;

    for (auto& frame : graph->thread_data)
    {
        frame.resize(get_job_worker_count());

        for (auto& thread : frame)
        {
            thread.cmd_pool = gpu_create_command_pool(g_renderer->device, cmd_pool_info);
            thread.cmd_count = 0;
        }
    }

    for (auto& fence : graph->fences)
    {
        fence = gpu_create_fence(g_renderer->device);
    }

    return graph;
}

void rg_destroy_render_graph(RenderGraph* graph)
{
    gpu_wait_for_fences(g_renderer->device, static_array_length(graph->fences), graph->fences, FenceWaitType::all);

    for (auto& fence : graph->fences)
    {
        gpu_destroy_fence(g_renderer->device, fence);
    }

    for (auto& frame : graph->thread_data)
    {
        for (auto& thread : frame)
        {
            gpu_destroy_command_pool(g_renderer->device, thread.cmd_pool);
            memset(thread.cmd_buffers, 0, sizeof(CommandBuffer) * static_array_length(thread.cmd_buffers));
        }
    }

    for (auto& resource : graph->resource_pool)
    {
        switch (resource.type)
        {
            case RenderGraphResourceType::buffer:
            {
                gpu_destroy_buffer(g_renderer->device, resource.buffer);
                break;
            }
            case RenderGraphResourceType::texture:
            {
                gpu_destroy_texture(g_renderer->device, resource.texture.handle);
                gpu_destroy_texture_view(g_renderer->device, resource.texture.view_handle);
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

    for (auto& pass : graph->pass_pool)
    {
        gpu_destroy_render_pass(g_renderer->device, pass.handle);
    }

    // unlink before deleting
    if (graph->prev != nullptr)
    {
        graph->prev->next = graph->next;
    }

    if (graph->next != nullptr)
    {
        graph->next->prev = graph->prev;
    }

    if (graph == g_renderer->first_graph)
    {
        g_renderer->first_graph = graph->next;
    }

    if (graph == g_renderer->last_graph)
    {
        g_renderer->last_graph = graph->prev;
    }

    BEE_DELETE(graph->allocator, graph);
}

RenderGraphThreadData& rg_get_thread_data(RenderGraph* graph)
{
    const auto frame = gpu_get_current_frame(g_renderer->device);
    const auto thread = get_local_job_worker_id();
    return graph->thread_data[frame][thread];
}

void rg_resolve_resource(RenderGraph* graph, PerFrameResource* src)
{
    // imported resources already have a GPU handle
    if (src->handle == RenderGraphResourceType::imported_buffer || src->handle == RenderGraphResourceType::imported_texture)
    {
        return;
    }

    const auto index = find_index_if(graph->resource_pool, [&](const PooledResource& r)
    {
        return r.hash == src->hash && r.type == src->handle;
    });

    if (src->handle == RenderGraphResourceType::backbuffer)
    {
        src->texture.view_handle = gpu_get_swapchain_texture_view(g_renderer->device, src->texture.swapchain);
        src->texture.handle = gpu_acquire_swapchain_texture(g_renderer->device, src->texture.swapchain);
        return;
    }

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
        resource->type = static_cast<RenderGraphResourceType>(src->handle.high());

        if (resource->type == RenderGraphResourceType::buffer)
        {
            resource->buffer = gpu_create_buffer(g_renderer->device, src->buffer.create_info);
        }
        else
        {
            auto& texture_info = src->texture.create_info;

            TextureViewCreateInfo view_info{};
            view_info.texture = gpu_create_texture(g_renderer->device, src->texture.create_info);
            view_info.type = texture_info.type;
            view_info.format = texture_info.format;
            view_info.mip_level_offset = 0;
            view_info.mip_level_count = texture_info.mip_count;
            view_info.array_element_offset = 0;
            view_info.array_element_count = texture_info.array_element_count;
            view_info.debug_name = texture_info.debug_name;

            resource->texture.handle = view_info.texture;
            resource->texture.view_handle = gpu_create_texture_view(g_renderer->device, view_info);
        }
    }

    if (resource->type == RenderGraphResourceType::buffer)
    {
        src->buffer.handle = resource->buffer;
    }
    else
    {
        src->texture.handle = resource->texture.handle;
        src->texture.view_handle = resource->texture.view_handle;
    }
}

void rg_resolve_pass(RenderGraph* graph, RenderGraphPass* pass)
{
    SubPassDescriptor subpass{};

    auto& pass_info = graph->tmp_pass_info;
    pass_info.attachment_count = sign_cast<u32>(pass->attachment_count);
    pass_info.subpass_count = 1;
    pass_info.subpasses = &subpass;

    for (int i = 0; i < pass->attachment_count; ++i)
    {
        pass_info.attachments[i] = pass->attachments[i];

        switch (pass->attachments[i].type)
        {
            case AttachmentType::present:
            case AttachmentType::color:
            {
                subpass.color_attachments[subpass.color_attachment_count] = sign_cast<u32>(i);
                ++subpass.color_attachment_count;

                // Resolve the pixel format for the color attachment from the texture
                auto& resource = graph->frame_resources[pass->attachment_textures[i].low()];

                // use the GPU backend to get the format instead of the create_info because this may be an imported texture
                pass_info.attachments[i].format = gpu_get_texture_format(g_renderer->device, resource.texture.handle);
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
        const auto new_pass = gpu_create_render_pass(g_renderer->device, pass_info);
        BEE_ASSERT(new_pass.is_valid());
        graph->pass_pool.push_back_no_construct();
        graph->pass_pool.back().hash = hash;
        graph->pass_pool.back().handle = new_pass;

        index = graph->pass_pool.size() - 1;
    }

    pass->handle = graph->pass_pool[index].handle;
}

void rg_execute_pass(RenderGraphPass* pass, std::atomic_int32_t* cmd_count)
{
    auto& thread = rg_get_thread_data(pass->graph);
    gpu_reset_command_pool(g_renderer->device, thread.cmd_pool);

    BEE_ASSERT(pass != nullptr);
    pass->execute_fn(pass->graph, &g_storage);

    for (int i = 0; i < thread.cmd_count; ++i)
    {
        if (thread.cmd_buffers[i].state() == CommandBufferState::recording)
        {
            thread.cmd_buffers[i].end();
        }

        if (thread.cmd_buffers[i].state() != CommandBufferState::empty)
        {
            cmd_count->fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void rg_execute_job(const i32 frame, RenderGraph* graph)
{
    JobGroup wait_handle{};

    // kick jobs or each pass

    std::atomic_int32_t cmd_count { 0 };

    for (int i = 0; i < graph->execute_count; ++i)
    {
        auto* job = create_job(rg_execute_pass, graph->execute_order[i], &cmd_count);
        job_schedule(&wait_handle, job);
    }

    job_wait(&wait_handle);

    const auto executed_cmd_count = cmd_count.load(std::memory_order_relaxed);

    if (executed_cmd_count > 0)
    {
        auto* cmd_buffers = BEE_ALLOCA_ARRAY(const CommandBuffer*, executed_cmd_count);

        int cmd_index = 0;

        for (auto& thread : graph->thread_data[frame])
        {
            for (int i = 0; i < thread.cmd_count; ++i)
            {
                if (thread.cmd_buffers[i].state() != CommandBufferState::empty)
                {
                    cmd_buffers[cmd_index] = &thread.cmd_buffers[i];
                    ++cmd_index;
                }
            }
        }

        SubmitInfo submit{};
        submit.fence = graph->fences[frame];
        submit.command_buffer_count = executed_cmd_count;
        submit.command_buffers = cmd_buffers;

        gpu_submit(&wait_handle, g_renderer->device, submit);
        job_wait(&wait_handle);

        for (int i = 0; i < graph->frame_resource_count; ++i)
        {
            if (graph->frame_resources[i].handle == RenderGraphResourceType::backbuffer)
            {
                gpu_present(g_renderer->device, graph->frame_resources[i].texture.swapchain);
            }
        }
    }

    // reset the graph
    graph->frame_pass_count = 0;
    graph->frame_resource_count = 0;
    graph->execute_count = 0;
}

void rg_execute(RenderGraph* graph, JobGroup* wait_handle)
{
    /*
     * TODO(Jacob):
     * validate:
     *  - for all passes
     *      - check inputs and outputs have same size
     *      - check all buffer/texture/blit inputs and outputs have same usage and size
     *      - check depth stencil input/output dimensions match
     */

    const auto frame = gpu_get_current_frame(g_renderer->device);
    gpu_wait_for_fence(g_renderer->device, graph->fences[frame]);

    FixedArray<PerFrameResource*> frontier(graph->frame_resource_count, temp_allocator());
    FixedArray<PerFrameResource*> resource_list(graph->frame_resource_count, temp_allocator());

    for (int i = 0; i < graph->frame_resource_count; ++i)
    {
        if (graph->frame_resources[i].refcount <= 0)
        {
            frontier.push_back(&graph->frame_resources[i]);
            resource_list.push_back(&graph->frame_resources[i]);
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

        for (int pass_index = 0; pass_index < resource->writer_pass_count; ++pass_index)
        {
            auto* pass = resource->writer_passes[pass_index];
            const auto pass_write_count = --pass->write_count;

            if (pass_write_count > 0)
            {
                // the pass still has resource dependencies
                continue;
            }

            // the pass has no write dependencies - check all the resources it reads from and then add to the final execute list
            for (int read_index = 0; read_index < pass->read_count; ++read_index)
            {
                auto& dependency = pass->reads[read_index];
                const auto resource_refcount = --graph->frame_resources[dependency.low()].refcount;

                if (resource_refcount > 0)
                {
                    // the resource still has dependent passes
                    continue;
                }

                // We've reached a leaf resource - so add to the frontier and mark as actually used
                frontier.push_back(&graph->frame_resources[dependency.low()]);
                resource_list.push_back(frontier.back());
            }

            // add to execute order - we've found a leaf pass
            graph->execute_order[graph->execute_count] = pass;
            ++graph->execute_count;
        }
    }

    BEE_ASSERT(graph->execute_count <= graph->frame_pass_count);

    // Execute order is now last pass->first - reverse to get it in order of first pass->last
    std::reverse(graph->execute_order, graph->execute_order + graph->execute_count);

    // Resolve all the resources and passes to their physical passes
    for (auto* resource : resource_list)
    {
        rg_resolve_resource(graph, resource);
    }

    // resolve all the passes
    for (int i = 0; i < graph->execute_count; ++i)
    {
        rg_resolve_pass(graph, graph->execute_order[i]);
    }

    auto* job = create_job(rg_execute_job, frame, graph);
    job_schedule(wait_handle, job);
}

/*
 ********************************************
 *
 * Render graph storage API
 *
 ********************************************
 */
BufferHandle rg_get_buffer(RenderGraph* graph, const RenderGraphResource& handle)
{
    if (BEE_FAIL(handle.low() < graph->frame_resource_count))
    {
        return BufferHandle{};
    }

    if (BEE_FAIL_F(handle == RenderGraphResourceType::buffer || handle == RenderGraphResourceType::imported_buffer, "Invalid resource handle"))
    {
        return BufferHandle{};
    }

    return graph->frame_resources[handle.low()].buffer.handle;
}

TextureHandle rg_get_texture(RenderGraph* graph, const RenderGraphResource& handle)
{
    if (BEE_FAIL(handle.low() < graph->frame_resource_count))
    {
        return TextureHandle{};
    }

    if (BEE_FAIL_F(handle == RenderGraphResourceType::texture || handle == RenderGraphResourceType::imported_texture, "Invalid resource handle"))
    {
        return TextureHandle{};
    }

    return graph->frame_resources[handle.low()].texture.handle;
}

CommandBuffer* rg_create_command_buffer(RenderGraph* graph, const QueueType queue)
{
    auto& thread_data = rg_get_thread_data(graph);

    if (thread_data.cmd_count >= rg_max_cmd)
    {
        log_error("Cannot create more than rg_max_cmd (%d) RenderGraph command buffers per thread per frame", rg_max_cmd);
        return nullptr;
    }

    auto* cmd = &thread_data.cmd_buffers[thread_data.cmd_count];
    if (cmd->native == nullptr)
    {
        new (cmd) CommandBuffer(g_renderer->device, thread_data.cmd_pool, queue);
    }
    cmd->begin(CommandBufferUsage::default_usage);

    ++thread_data.cmd_count;

    return cmd;
}

/*
 ********************************************
 *
 * Render graph pass API
 *
 ********************************************
 */
template <>
struct Hash<BufferHandle>
{
    inline u32 operator()(const BufferHandle& key)
    {
        return key.id;
    }
};

template <>
struct Hash<TextureHandle>
{
    inline u32 operator()(const TextureHandle& key)
    {
        return key.id;
    }
};

template <typename T>
inline RenderGraphResource rg_add_resource(RenderGraph* graph, const char* name, const RenderGraphResourceType type, const T& create_info_or_handle)
{
    if (graph->frame_resource_count >= rg_max_resources)
    {
        log_error("Cannot create more than rg_max_resources (%d) RenderGraph resources in a single frame", rg_max_resources);
        return RenderGraphResource{};
    }

    const auto index = graph->frame_resource_count;
    ++graph->frame_resource_count;

    auto& resource = graph->frame_resources[index];
    resource.hash = get_hash(create_info_or_handle);
    resource.name = name;
    resource.handle = RenderGraphResource(index, underlying_t(type));
    resource.writer_pass_count = 0;
    resource.refcount = 0;

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
            memcpy(&resource.texture.swapchain, &create_info_or_handle, sizeof(T));
            break;
        }
        default:
        {
            BEE_UNREACHABLE("Not implemented");
        }
    }

    return resource.handle;
}

RenderGraphResource rg_create_buffer(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info)
{
    return rg_add_resource(pass->graph, name, RenderGraphResourceType::buffer, create_info);
}

RenderGraphResource rg_create_texture(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info)
{
    return rg_add_resource(pass->graph, name, RenderGraphResourceType::texture, create_info);
}

RenderGraphResource rg_import_buffer(RenderGraphPass* pass, const char* name, const BufferHandle& buffer)
{
    return rg_add_resource(pass->graph, name, RenderGraphResourceType::imported_buffer, buffer);
}

RenderGraphResource rg_import_texture(RenderGraphPass* pass, const char* name, const TextureHandle& texture)
{
    return rg_add_resource(pass->graph, name, RenderGraphResourceType::imported_texture, texture);
}

RenderGraphResource rg_import_backbuffer(RenderGraphPass* pass, const char* name, const SwapchainHandle& swapchain)
{
    return rg_add_resource(pass->graph, name, RenderGraphResourceType::backbuffer, swapchain);
}

void rg_write_resource(RenderGraphPass* pass, const RenderGraphResource& resource)
{
    BEE_ASSERT(pass != nullptr);

    auto& pooled = pass->graph->frame_resources[resource.low()];

    if (pooled.writer_pass_count >= rg_pass_max_ops)
    {
        log_error("Cannot write more than rg_pass_max_ops resources (%d) to the one RenderGraph pass", rg_pass_max_ops);
        return;
    }

    ++pass->write_count;
    ++pooled.writer_pass_count;
    pooled.writer_passes[pooled.writer_pass_count - 1] = pass;
}

void rg_read_resource(RenderGraphPass* pass, const RenderGraphResource& resource)
{
    BEE_ASSERT(pass != nullptr);

    if (pass->read_count >= rg_pass_max_ops)
    {
        log_error("Cannot read more than rg_pass_max_ops resources (%d) in the one RenderGraph pass", rg_pass_max_ops);
        return;
    }

    auto& pooled = pass->graph->frame_resources[resource.low()];
    ++pooled.refcount;

    pass->reads[pass->read_count] = resource;
    pass->read_count++;
}

bool rg_add_attachment(RenderGraphPass* pass, const RenderGraphResource& texture, const AttachmentDescriptor& desc)
{
    BEE_ASSERT(pass != nullptr);

    // if the attachment texture is already added we need to replace the attachment stored in the pass with the new one
    for (int i = 0; i < pass->attachment_count; ++i)
    {
        if (pass->attachment_textures[i] == texture)
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
    pass->attachment_textures[pass->attachment_count] = texture;
    ++pass->attachment_count;
    return true;
}

void rg_write_color(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store, const u32 samples)
{
    BEE_ASSERT(texture != RenderGraphResourceType::buffer && texture != RenderGraphResourceType::imported_buffer);

    AttachmentDescriptor desc{};
    desc.type = AttachmentType::color;
    desc.format = PixelFormat::unknown; // we'll get the color later from the texture format
    desc.load_op = load;
    desc.store_op = store;
    desc.samples = samples;

    if (rg_add_attachment(pass, texture, desc))
    {
        rg_write_resource(pass, texture);
    }
}

void rg_write_depth(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store)
{
    BEE_ASSERT(texture != RenderGraphResourceType::buffer && texture != RenderGraphResourceType::imported_buffer);
    BEE_ASSERT_F(is_depth_format(depth_format), "depth_format is not a valid depth-stencil pixel format");

    AttachmentDescriptor desc{};
    desc.type = AttachmentType::depth_stencil;
    desc.format = depth_format;
    desc.load_op = load;
    desc.store_op = store;
    desc.samples = 1;

    if (rg_add_attachment(pass, texture, desc))
    {
        rg_write_resource(pass, texture);
    }
}

void rg_set_execute_function(RenderGraphPass* pass, render_graph_execute_t&& fn)
{
    if (BEE_FAIL_F(!pass->has_execute, "RenderGraph pass already has an execute function assigned"))
    {
        return;
    }

    pass->execute_fn = std::move(fn);
    pass->has_execute = true;

}

RenderGraphPass* rg_add_pass(RenderGraph* graph, const char* name)
{
    if (graph->frame_pass_count >= rg_max_passes)
    {
        log_error("Cannot add more than rg_max_passes (%d) RenderGraph passes in a single frame", rg_max_passes);
        return nullptr;
    }

    auto* pass = &graph->frame_passes[graph->frame_pass_count];
    ++graph->frame_pass_count;

    BEE_ASSERT(graph->frame_pass_count <= 1);

    pass->attachment_count = 0;
    pass->read_count = 0;
    pass->write_count = 0;
    pass->graph = graph;
    pass->has_execute = false;

    return pass;
}

DeviceHandle rg_get_device()
{
    return g_renderer->device;
}

i32 get_swapchains(SwapchainHandle* dst)
{
    if (dst != nullptr)
    {
        for (int i = 0; i < g_renderer->swapchain_count; ++i)
        {
            dst[i] = g_renderer->swapchains[i].handle;
        }
    }

    return g_renderer->swapchain_count;
}

SwapchainHandle get_primary_swapchain()
{
    return g_renderer->swapchain_count > 0 ? g_renderer->swapchains[0].handle : SwapchainHandle{};
}

/*
 *********************************
 *
 * Renderer module implementation
 *
 *********************************
 */
bool operator<(const RegisteredSwapchain& lhs, const RegisteredSwapchain& rhs)
{
    if (lhs.kind != rhs.kind)
    {
        return lhs.kind < rhs.kind;
    }

    if (lhs.id != rhs.id)
    {
        // we want the last-created swapchains to be at the front of the list
        return lhs.id > rhs.id;
    }

    return lhs.hash < rhs.hash;
}

bool init_renderer(const DeviceCreateInfo& device_info)
{
    if (g_renderer->device.is_valid())
    {
        log_error("Renderer is already initialized");
        return false;
    }

    g_renderer->device = gpu_create_device(device_info);

    if (BEE_FAIL(g_renderer->device.is_valid()))
    {
        return false;
    }

    // initialize the default render graph
    g_renderer->default_graph = rg_create_render_graph(system_allocator());

    if (BEE_FAIL(g_renderer->default_graph != nullptr))
    {
        return false;
    }

    // Initialize any render stages added before the renderer was initialized
    for (auto& stage : g_renderer->stages)
    {
        stage->init(g_renderer->device);
    }

    return true;
}

void destroy_renderer()
{
    if (BEE_FAIL_F(g_renderer->device.is_valid(), "Renderer is already destroyed or uninitialized"))
    {
        return;
    }

    gpu_device_wait(g_renderer->device);

    while (g_renderer->first_graph != nullptr)
    {
        rg_destroy_render_graph(g_renderer->first_graph);
    }

    g_renderer->default_graph = nullptr;

    for (auto* stage : g_renderer->stages)
    {
        stage->destroy(g_renderer->device);
    }

    g_renderer->stages.clear();

    for (int i = 0; i < g_renderer->swapchain_count; ++i)
    {
        gpu_destroy_swapchain(g_renderer->device, g_renderer->swapchains[i].handle);
    }

    gpu_destroy_device(g_renderer->device);
    g_renderer->device = DeviceHandle{};
    g_renderer->swapchain_count = 0;
}

void execute_frame()
{
    for (auto* stage : g_renderer->stages)
    {
        stage->execute(g_renderer->default_graph, &g_builder);
    }

    JobGroup wait_handle{};
    rg_execute(g_renderer->default_graph, &wait_handle);
    job_wait(&wait_handle);

    gpu_commit_frame(g_renderer->device);
}

void add_stage(RenderStage* stage)
{
    const auto index = find_index(g_renderer->stages, stage);

    if (index >= 0)
    {
        g_renderer->stages[index] = stage;
    }
    else
    {
        g_renderer->stages.push_back(stage);

        if (g_renderer->device.is_valid())
        {
            stage->init(g_renderer->device);
        }
    }
}

void remove_stage(RenderStage* stage)
{
    const auto index = find_index(g_renderer->stages, stage);

    if (index >= 0)
    {
        if (g_renderer->device.is_valid())
        {
            g_renderer->stages[index]->destroy(g_renderer->device);
        }

        g_renderer->stages.erase(index);
    }
}

DeviceHandle get_renderer_device()
{
    return g_renderer->device;
}

void add_renderer_swapchain(const SwapchainKind kind, const WindowHandle& window, const PixelFormat format, const char* name)
{
    if (BEE_FAIL_F(g_renderer->swapchain_count < max_swapchains, "Cannot add more than max_swapchains (%d) to renderer", max_swapchains))
    {
        return;
    }

    SwapchainCreateInfo create_info{};
    create_info.texture_format = format;
    create_info.texture_extent = Extent::from_platform_size(get_window_framebuffer_size(window));
    create_info.texture_usage = TextureUsage::color_attachment;
    create_info.texture_array_layers = 1;
    create_info.vsync = true;
    create_info.window = window;
    create_info.debug_name = name;

    const auto handle = gpu_create_swapchain(g_renderer->device, create_info);

    if (!handle.is_valid())
    {
        log_error("Failed to add swapchain to renderer");
        return;
    }

    const auto name_hash = get_hash(name);
    const auto index = g_renderer->swapchain_count;
    ++g_renderer->swapchain_count;

    auto& swapchain = g_renderer->swapchains[index];
    swapchain.id = index;
    swapchain.kind = kind;
    swapchain.handle = handle;
    swapchain.name = name;
    swapchain.hash = name_hash;

    if (kind == SwapchainKind::primary)
    {
        std::sort(g_renderer->swapchains, g_renderer->swapchains + g_renderer->swapchain_count);
    }
}

void remove_renderer_swapchain(const char* name)
{
    const auto hash = get_hash(name);
    const auto index = find_index_if(
        g_renderer->swapchains,
        g_renderer->swapchains + g_renderer->swapchain_count,
        [&](const RegisteredSwapchain& s)
        {
            return s.hash == hash;
        }
    );

    if (index < 0)
    {
        log_error("Swapchain \"%s\" was not added to the renderer", name);
        return;
    }

    gpu_destroy_swapchain(g_renderer->device, g_renderer->swapchains[index].handle);

    // swap the old swapchain to the back, decrement size
    std::swap(g_renderer->swapchains[index], g_renderer->swapchains[g_renderer->swapchain_count - 1]);
    --g_renderer->swapchain_count;

    // re-sort the swapchains
    std::sort(g_renderer->swapchains, g_renderer->swapchains + g_renderer->swapchain_count);
}


} // namespace bee


static bee::RendererModule      g_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_renderer = registry->get_or_create_persistent<bee::Renderer>("BeeRenderer");

    // Renderer
    g_module.init = bee::init_renderer;
    g_module.destroy = bee::destroy_renderer;
    g_module.execute_frame = bee::execute_frame;
    g_module.add_stage = bee::add_stage;
    g_module.remove_stage = bee::remove_stage;
    g_module.get_device = bee::get_renderer_device;
    g_module.add_swapchain = bee::add_renderer_swapchain;
    g_module.remove_swapchain = bee::remove_renderer_swapchain;
    registry->toggle_module(state, BEE_RENDERER_MODULE_NAME, &g_module);

    // RenderGraphBuilder
    bee::g_builder.create_render_graph = bee::rg_create_render_graph;
    bee::g_builder.destroy_render_graph = bee::rg_destroy_render_graph;
    bee::g_builder.create_buffer = bee::rg_create_buffer;
    bee::g_builder.create_texture = bee::rg_create_texture;
    bee::g_builder.import_buffer = bee::rg_import_buffer;
    bee::g_builder.import_texture = bee::rg_import_texture;
    bee::g_builder.import_backbuffer = bee::rg_import_backbuffer;
    bee::g_builder.write_color = bee::rg_write_color;
    bee::g_builder.write_depth = bee::rg_write_depth;
    bee::g_builder.set_execute_function = bee::rg_set_execute_function;
    bee::g_builder.add_pass = bee::rg_add_pass;
    bee::g_builder.get_device = bee::rg_get_device;
    bee::g_builder.get_swapchains = bee::get_swapchains;
    bee::g_builder.get_primary_swapchain = bee::get_primary_swapchain;
    registry->toggle_module(state, BEE_RENDER_GRAPH_BUILDER_MODULE_NAME, &bee::g_builder);

    // RenderGraphStorage
    bee::g_storage.get_buffer = bee::rg_get_buffer;
    bee::g_storage.get_texture = bee::rg_get_texture;
    bee::g_storage.create_command_buffer = bee::rg_create_command_buffer;
}