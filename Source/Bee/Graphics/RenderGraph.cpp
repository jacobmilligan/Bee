/*
 *  RenderGraph.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Graphics/RenderGraph.hpp"

#include <algorithm>

namespace bee {


/*
 ****************************************
 *
 * RenderGraphBuilder - implementation
 *
 ****************************************
 */
RenderGraphBuilder::RenderGraphBuilder(RenderGraph* graph, const i32 pass_index)
    : graph_(graph),
      pass_index_(pass_index)
{
}

RenderGraphResource RenderGraphBuilder::create_buffer(const char* name, const BufferCreateInfo& create_info)
{
    const auto handle = graph_->get_or_create_buffer(name, create_info);
    BEE_ASSERT(handle.is_valid());
    return handle;
}

RenderGraphResource RenderGraphBuilder::create_texture(const char* name, const TextureCreateInfo& create_info)
{
    const auto handle = graph_->get_or_create_texture(name, create_info);
    BEE_ASSERT(handle.is_valid());
    return handle;
}

RenderGraphBuilder&
RenderGraphBuilder::write_color(const RenderGraphResource& texture, const LoadOp load, const StoreOp store)
{
    BEE_ASSERT(texture.is(RenderGraphResourceType::texture) || texture.is(RenderGraphResourceType::imported_texture));

    AttachmentDescriptor desc{};
    desc.samples = 1;
    desc.load_op = load;
    desc.store_op = store;
    desc.format = PixelFormat::unknown; // get format from texture format

    if (graph_->add_attachment(pass_index_, texture, RenderGraphAttachmentType::color, desc))
    {
        graph_->write_resource(pass_index_, texture);
    }
    else
    {
        log_warning("RenderGraphBuilder: failed to write color attachment - exceeded BEE_GPU_MAX_ATTACHMENTS");
    }

    return *this;
}

RenderGraphBuilder&
RenderGraphBuilder::write_depth(const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load,
                                const StoreOp store)
{
    BEE_ASSERT(texture.is(RenderGraphResourceType::texture) || texture.is(RenderGraphResourceType::imported_texture));
    BEE_ASSERT_F(is_depth_format(depth_format), "depth_format is not a valid depth-stencil pixel format");

    AttachmentDescriptor desc{};
    desc.samples = 1;
    desc.load_op = load;
    desc.store_op = store;
    desc.format = depth_format;

    if (graph_->add_attachment(pass_index_, texture, RenderGraphAttachmentType::depth, desc))
    {
        graph_->write_resource(pass_index_, texture);
    }
    else
    {
        log_warning("RenderGraphBuilder: failed to write depth attachment - exceeded BEE_GPU_MAX_ATTACHMENTS");
    }


    return *this;
}


/*
 ************************************************
 *
 * RenderGraphExecuteContext - implementation
 *
 ************************************************
 */
RenderGraphExecuteContext::RenderGraphExecuteContext(RenderGraph* graph, const RenderPassHandle& pass)
    : graph_(graph),
      pass_(pass)
{
}

BufferHandle RenderGraphExecuteContext::get_buffer(const RenderGraphResource& handle) const
{
    return graph_->get_physical_buffer(handle);
}

TextureHandle RenderGraphExecuteContext::get_texture(const RenderGraphResource& handle) const
{
    return graph_->get_physical_texture(handle);
}


/*
 ****************************************
 *
 * RenderGraph - implementation
 *
 ****************************************
 */
RenderGraph::RenderGraph(const DeviceHandle& device)
    : device_(device),
      command_batcher_(device),
      per_worker_command_allocators_(get_job_worker_count())
{
    for (int i = 0; i < get_job_worker_count(); ++i)
    {
        per_worker_command_allocators_.emplace_back(kibibytes(1));
    }
}

RenderGraph::~RenderGraph()
{
    for (auto& pass : physical_passes_.handles)
    {
        gpu_destroy_render_pass(device_, pass);
    }

    for (auto& buffer : buffers_.resources)
    {
        gpu_destroy_buffer(device_, buffer);
    }

    for (auto& texture : textures_.resources)
    {
        gpu_destroy_texture_view(device_, texture.view);
        gpu_destroy_texture(device_, texture.handle);
    }
}

RenderGraphResource
RenderGraph::add_active_resource(ActiveResourceList* list, const i32 index, const RenderGraphResourceType& type)
{
    const auto new_size = ++list->size;
    const auto new_index = new_size - 1;
    if (new_size >= list->physical_indices.size())
    {
        list->physical_indices.resize(new_size);
        list->types.resize(new_size);
        list->reference_counts.resize(new_size);
        list->writer_passes.resize(new_size);
        list->writer_passes[new_index] = DynamicArray<i32>(list->allocator);
    }

    list->physical_indices[new_index] = index;
    list->types[new_index] = type;
    list->reference_counts[new_index] = 0;
    list->writer_passes[new_index].clear();

    return RenderGraphResource(new_index, type);
}

RenderPassHandle RenderGraph::obtain_physical_pass(const DeviceHandle& device, PhysicalPassPool* pool,
                                                   const RenderPassCreateInfo& create_info)
{
    const auto hash = get_hash(create_info);
    const auto index = find_index_if(pool->hashes, [&](const u32 stored_hash)
    {
        return stored_hash == hash;
    });

    if (index >= 0)
    {
        return pool->handles[index];
    }

    const auto handle = gpu_create_render_pass(device, create_info);
    pool->hashes.push_back(hash);
    pool->handles.push_back(handle);

    return handle;
}

RenderGraphBuilder RenderGraph::add_pass(const char* name)
{
    if (next_pass_ >= passes_.size())
    {
        passes_.emplace_back();
    }

    passes_[next_pass_].reset(name);
    ++next_pass_;

    return RenderGraphBuilder(this, next_pass_ - 1);
}

RenderGraphResource RenderGraph::get_or_create_buffer(const char* name, const BufferCreateInfo& create_info)
{
    const auto index = buffers_.get_or_create(name, active_list_.size, create_info);
    return add_active_resource(&active_list_, index, RenderGraphResourceType::buffer);
}

RenderGraphResource RenderGraph::get_or_create_texture(const char* name, const TextureCreateInfo& create_info)
{
    const auto index = textures_.get_or_create(name, active_list_.size, create_info);
    return add_active_resource(&active_list_, index, RenderGraphResourceType::texture);
}

BufferHandle RenderGraph::get_physical_buffer(const RenderGraphResource& handle) const
{
    return BufferHandle{};
}

TextureHandle RenderGraph::get_physical_texture(const RenderGraphResource& handle) const
{
    return TextureHandle{};
}

void RenderGraph::write_resource(const i32 pass_index, const RenderGraphResource& handle)
{
    BEE_ASSERT(pass_index < next_pass_);

    auto& pass = passes_[pass_index];
    ++pass.write_count;
    active_list_.writer_passes[handle.index].push_back(pass_index);
}

void RenderGraph::read_resource(const i32 pass_index, const RenderGraphResource& handle)
{
    BEE_ASSERT(pass_index < next_pass_);

    ++active_list_.reference_counts[handle.index];

    auto& pass = passes_[pass_index];
    const auto index_in_pass = pass.read_count++;
    pass.resources_read[index_in_pass] = handle;
}

bool RenderGraph::add_attachment(const i32 pass_index, const RenderGraphResource& texture, const RenderGraphAttachmentType type, const AttachmentDescriptor& desc)
{
    BEE_ASSERT(pass_index < next_pass_);
    auto& pass = passes_[pass_index];

    // find an existing texture->attachment mapping
    for (u32 att = 0; att < pass.info.attachment_count; ++att)
    {
        if (pass.attachment_textures[att] == texture)
        {
            pass.attachment_types[att] = type;
            pass.info.attachments[att] = desc;
            return true;
        }
    }

    if (pass.info.attachment_count >= BEE_GPU_MAX_ATTACHMENTS)
    {
        return false;
    }

    // Add a new attachment using the texture resource
    pass.info.attachments[pass.info.attachment_count] = desc;
    pass.attachment_textures[pass.info.attachment_count] = texture;
    pass.attachment_types[pass.info.attachment_count] = type;
    ++pass.info.attachment_count;
    return true;
}


struct ExecutePassJob final : public Job
{
    RenderGraph*        graph { nullptr };
    RenderGraphPass*    pass { nullptr };

    void execute() override;
};


void RenderGraph::execute(JobGroup *wait_handle)
{
    /*
    * TODO(Jacob):
    * validate:
    *  - for all passes
    *      - check inputs and outputs have same size
    *      - check all buffer/texture/blit inputs and outputs have same usage and size
    *      - check depth stencil input/output dimensions match
    */
    FixedArray<RenderGraphPass*> execute_order(active_list_.size, temp_allocator());
    FixedArray<i32> frontier(active_list_.size, temp_allocator());
    FixedArray<RenderGraphResource> final_resource_list(active_list_.size, temp_allocator());

    for (int r = 0; r < active_list_.size; ++r)
    {
        if (active_list_.reference_counts[r] <= 0)
        {
            frontier.push_back(r);
            final_resource_list.emplace_back(r, active_list_.types[r]);
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
        const auto resource = frontier.back();
        frontier.pop_back();

        for (const auto& pass : active_list_.writer_passes[resource])
        {
            const auto pass_write_count = --passes_[pass].write_count;

            if (pass_write_count <= 0)
            {
                // the pass still has resource dependencies
                continue;
            }

            // the pass has no write dependencies - check all the resources it reads from and then add to the final execute list
            for (const auto& dependency : passes_[pass].resources_read)
            {
                const auto resource_refcount = --active_list_.reference_counts[dependency.index];

                if (resource_refcount > 0)
                {
                    // the resource still has dependent passes
                    continue;
                }

                // We've reached a leaf resource - so add to the frontier and mark as actually used
                frontier.push_back(dependency.index);
                final_resource_list.push_back(dependency);
            }

            // add to execute order - we've found a lead pass
            execute_order.push_back(&passes_[pass]);
        }
    }

    // Execute order is now last pass->first - reverse to get it in order of first pass->last
    std::reverse(execute_order.begin(), execute_order.end());

    // This has to be on the main thread because we're potentially creating GPU resources here
    for (const auto& resource : final_resource_list)
    {
        const auto physical_index = active_list_.physical_indices[resource.index];

        switch (resource.type)
        {
            case RenderGraphResourceType::texture:
            {
                auto& texture = textures_.resources[physical_index];
                if (!texture.handle.is_valid())
                {
                    texture.handle = gpu_create_texture(device_, textures_.create_infos[physical_index]);
                }
                // if (is_attachment) create_view
                break;
            }
            case RenderGraphResourceType::buffer:
            {
                auto& buffer = buffers_.resources[physical_index];
                if (!buffer.is_valid())
                {
                    buffer = gpu_create_buffer(device_, buffers_.create_infos[physical_index]);
                }
                break;
            }
            case RenderGraphResourceType::imported_buffer:
            case RenderGraphResourceType::imported_texture:
            {
                // imported gpu resources already have their handle attached to the render graph resource
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Invalid type");
            }
        }
    }

    // again, this must be on the main thread because we might end up creating a new GPU render pass resource
    for (auto pass : execute_order)
    {
        for (u32 att = 0; att < pass->info.attachment_count; ++att)
        {
            auto& subpass = pass->subpasses[0];
            switch (pass->attachment_types[att])
            {
                case RenderGraphAttachmentType::color:
                {
                    ++subpass.color_attachment_count;
                    subpass.color_attachments[subpass.color_attachment_count - 1] = static_cast<u32>(att);

                    // Get the pixel format from the texture create info for color attachments
                    const auto virtual_texture_index = pass->attachment_textures[att].index;
                    const auto physical_texture_index = active_list_.physical_indices[virtual_texture_index];
                    pass->info.attachments[att].format = textures_.create_infos[physical_texture_index].format;
                    break;
                }
                case RenderGraphAttachmentType::depth:
                {
                    subpass.depth_stencil = static_cast<u32>(att);
                    break;
                }
                case RenderGraphAttachmentType::input:
                {
                    ++subpass.input_attachment_count;
                    subpass.input_attachments[subpass.input_attachment_count - 1] = static_cast<u32>(att);
                    break;
                }
                default:
                {
                    BEE_UNREACHABLE("invalid attachment type");
                }
            }
        }

        pass->physical_pass = obtain_physical_pass(device_, &physical_passes_, pass->info);
        BEE_ASSERT(pass->physical_pass.is_valid());

//        auto job = allocate_job<ExecutePassJob>();
//        job->graph = this;
//        job->pass = pass;
//        job_schedule(wait_handle, job);
    }
}


void ExecutePassJob::execute()
{
    RenderGraphExecuteContext ctx(graph, pass->physical_pass);
    pass->execute(&ctx);
}


} // namespace bee