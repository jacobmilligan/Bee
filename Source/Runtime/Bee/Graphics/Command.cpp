/*
 *  Command.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Sort.hpp"
#include "Bee/Graphics/Command.hpp"


namespace bee {


CommandAllocator::CommandAllocator(const size_t chunk_size)
    : chunk_size_(chunk_size)
{}

CommandAllocator::~CommandAllocator()
{
    free_all_chunks();
    free_chunks_ = nullptr;
}

CommandChunk* CommandAllocator::allocate()
{
    CommandChunk* chunk = free_chunks_;

    if (chunk != nullptr)
    {
        free_chunks_ = free_chunks_->next;
    }
    else
    {
        auto data = static_cast<u8*>(BEE_MALLOC_ALIGNED(system_allocator(), chunk_size_, alignof(CommandChunk)));
        auto header = reinterpret_cast<Header*>(data);
        header->next = chunks_;

        if (chunks_ == nullptr)
        {
            chunks_ = header;
        }
        else
        {
            chunks_->previous = header;
        }

        chunk = reinterpret_cast<CommandChunk*>(data + sizeof(Header));
        chunk->capacity = chunk_size_ - sizeof(Header) - sizeof(CommandChunk);
        chunk->data = data + sizeof(Header) + sizeof(CommandChunk);
    }

    chunk->size = 0;
    chunk->next = nullptr;

    return chunk;
}

void CommandAllocator::deallocate(CommandChunk* chunk)
{
    chunk->next = free_chunks_;
    chunk->size = 0;
    chunk->capacity = 0;
    free_chunks_ = chunk;
}

void CommandAllocator::trim()
{
    CommandChunk* chunk = free_chunks_;

    while (chunk != nullptr)
    {
        auto header = reinterpret_cast<Header*>(reinterpret_cast<u8*>(chunk) - sizeof(Header));

        if (header->previous != nullptr)
        {
            header->previous->next = header->next;
        }
        if (header->next != nullptr)
        {
            header->next->previous = header->previous;
        }

        chunk = chunk->next;

        BEE_FREE(system_allocator(), header);
    }
}

void CommandAllocator::free_all_chunks()
{
    while (chunks_ != nullptr)
    {
        auto next_chunk = chunks_->next;
        BEE_FREE(system_allocator(), chunks_);
        chunks_ = next_chunk;
    }
}

CommandStream::CommandStream(CommandAllocator* allocator)
    : headers_(system_allocator()), // TODO(Jacob): this should be using a more specific pool-local allocator
      allocator_(allocator)
{}

void CommandStream::push_chunk()
{
    current_chunk_ = allocator_->allocate();

    if (first_chunk_ == nullptr)
    {
        first_chunk_ = last_chunk_ = current_chunk_;
    }
    else
    {
        last_chunk_->next = current_chunk_;
    }
}

void* CommandStream::allocate_command(const GpuCommandType type, const QueueType queue_type, const size_t size)
{
    if (current_chunk_ == nullptr || current_chunk_->size + size >= current_chunk_->capacity)
    {
        push_chunk();
    }

    headers_.emplace_back();
    headers_.back().sort_key = 0;
    headers_.back().type = type;
    headers_.back().queue_type = queue_type;
    headers_.back().data = current_chunk_->data + current_chunk_->size;

    current_chunk_->size += size;

    return headers_.back().data;
}

CommandStream::~CommandStream()
{
    free_chunks();
}

void CommandStream::reset(const CommandStreamReset reset_type)
{
    headers_.clear();

    if (reset_type != CommandStreamReset::release_resources)
    {
        current_chunk_ = first_chunk_;
        while (current_chunk_ != nullptr)
        {
            current_chunk_->size = 0;
            current_chunk_ = current_chunk_->next;
        }
        current_chunk_ = first_chunk_;
    }
    else
    {
        free_chunks();
        headers_.shrink_to_fit();
    }
}

void CommandStream::free_chunks() noexcept
{
    while (first_chunk_ != nullptr)
    {
        auto next = first_chunk_->next;
        allocator_->deallocate(first_chunk_);
        first_chunk_ = next;
    }

    first_chunk_ = nullptr;
    last_chunk_ = nullptr;
    current_chunk_ = nullptr;
}

CommandBuffer::CommandBuffer(CommandAllocator* allocator)
    : stream_(allocator)
{}

CommandBuffer::~CommandBuffer()
{
    is_in_pass_ = false;
    queue_mask_ = QueueType::none;
    new (&current_draw_) DrawItem{};
}

void CommandBuffer::begin_render_pass(
    const RenderPassHandle&     pass,
    const u32                   attachment_count,
    const TextureViewHandle*    attachments,
    const RenderRect&           render_area,
    const u32                   clear_value_count,
    const ClearValue*           clear_values
)
{
    BEE_ASSERT(attachment_count <= BEE_GPU_MAX_ATTACHMENTS);
    BEE_ASSERT(clear_value_count <= BEE_GPU_MAX_ATTACHMENTS);

    auto cmd = push_command<CmdBeginRenderPass>();
    cmd->pass = pass;
    cmd->render_area = render_area;
    cmd->attachment_count = attachment_count;
    cmd->clear_value_count = clear_value_count;
    memcpy(cmd->attachments, attachments, sizeof(TextureViewHandle) * attachment_count);
    memcpy(cmd->clear_values, clear_values, sizeof(ClearValue) * clear_value_count);

    is_in_pass_ = true;
}

void CommandBuffer::end_render_pass()
{
    BEE_ASSERT_F(is_in_pass_, "CommandBuffer: cannot call `end_render_pass` without calling `begin_render_pass` first");
    push_command<CmdEndRenderPass>();

    is_in_pass_ = false;
}

void CommandBuffer::bind_pipeline_state(const PipelineStateHandle& pipeline)
{
    current_draw_.pipeline = pipeline;
}

void CommandBuffer::bind_vertex_buffer(const BufferHandle& buffer, const u32 binding, const u32 offset)
{
    bind_vertex_buffers(binding, 1, &buffer, &offset);
}

void CommandBuffer::bind_vertex_buffers(const u32 first_binding, const u32 count, const BufferHandle* buffers, const u32* offsets)
{
    BEE_ASSERT_F(current_draw_.vertex_buffer_count + count <= BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS, "CommandBuffer: cannot bind > BEE_GPU_MAX_VERTEX_BUFFER_BINDINGS vertex buffers at a time");

    for (u32 b = 0; b < count; ++b)
    {
        auto index = container_index_of(current_draw_.vertex_buffer_bindings, [&](const u32 binding) { return binding == first_binding + b; });
        if (index < 0)
        {
            index = current_draw_.vertex_buffer_count++;
        }

        current_draw_.vertex_buffer_offsets[index] = offsets[b];
        current_draw_.vertex_buffers[index] = buffers[b];
        current_draw_.vertex_buffer_bindings[index] = first_binding + b;
    }
}

void CommandBuffer::bind_index_buffer(const BufferHandle& buffer, const u32 offset, const IndexFormat index_format)
{
    current_draw_.index_buffer = buffer;
    current_draw_.index_buffer_offset = offset;
    current_draw_.index_buffer_format = index_format;
}

void CommandBuffer::copy_buffer(const BufferHandle& src, const i32 src_offset, const BufferHandle& dst, const i32 dst_offset, const i32 size)
{
    BEE_ASSERT_F(src.is_valid(), "CommandBuffer: copying from an invalid source buffer");
    BEE_ASSERT_F(dst.is_valid(), "CommandBuffer: copying to an invalid destination buffer");

    auto cmd = push_command<CmdCopyBuffer>();
    cmd->src = src;
    cmd->dst = dst;
    cmd->src_offset = src_offset;
    cmd->dst_offset = dst_offset;
    cmd->size = size;
}


void CommandBuffer::draw(const u32 vertex_count, const u32 instance_count, const u32 first_vertex, const u32 first_instance)
{
    auto cmd = push_command<CmdDraw>();
    cmd->first_vertex = first_vertex;
    cmd->vertex_count = vertex_count;
    cmd->first_instance = first_instance;
    cmd->instance_count = instance_count;
    memcpy(&cmd->item, &current_draw_, sizeof(DrawItem));

    new (&current_draw_) DrawItem{};
}

void CommandBuffer::draw_indexed(const u32 index_count, const u32 instance_count, const u32 vertex_offset, const u32 first_index, const u32 first_instance)
{
    auto cmd = push_command<CmdDrawIndexed>();
    cmd->first_index = first_index;
    cmd->index_count = index_count;
    cmd->vertex_offset = vertex_offset;
    cmd->first_instance = first_instance;
    cmd->instance_count = instance_count;
    memcpy(&cmd->item, &current_draw_, sizeof(DrawItem));

    new (&current_draw_) DrawItem{};
}

void CommandBuffer::set_viewport(const Viewport& viewport)
{
    current_draw_.viewport = viewport;
}

void CommandBuffer::set_scissor(const RenderRect& scissor)
{
    current_draw_.scissor = scissor;
}

void CommandBuffer::transition_resources(const u32 count, const GpuTransition* transitions)
{
    GpuTransition* data = nullptr;
    auto cmd = push_command_with_dynamic_data<CmdTransitionResources>(&data);

    memcpy(data, transitions, sizeof(GpuTransition) * count);

    cmd->count = count;
    cmd->transitions = data;
}

void CommandBuffer::end()
{
    push_command<CmdEnd>();
}

/*
 *************************
 *
 * Command compiler jobs
 *
 *************************
 */

#define BEE_GPU_CMD(CmdType)                                    \
    case CmdType::command_type:                                 \
    {                                                           \
        auto data = static_cast<CmdType*>(header.data);         \
        gpu_record_command(command_buffer, *data);              \
        break;                                                  \
    }

void CommandBatcher::compile_commands_job(CommandBatcher::CompileCommandsArgs* args)
{
    auto& pool = args->batcher->per_worker_pools_[get_local_job_worker_id()];
    ++pool.version;

    auto command_buffer = pool.obtain(args->queue, args->fence);

    gpu_begin_command_buffer(command_buffer);

    for (int cmd = 0; cmd < args->commands_count; ++cmd)
    {
        auto& header = args->commands[cmd];
        switch (header.type)
        {
            BEE_GPU_CMD(CmdBeginRenderPass)
            BEE_GPU_CMD(CmdEndRenderPass)
            BEE_GPU_CMD(CmdCopyBuffer)
            BEE_GPU_CMD(CmdDraw)
            BEE_GPU_CMD(CmdDrawIndexed)
            BEE_GPU_CMD(CmdTransitionResources)
            default:
            {
                BEE_UNREACHABLE("Invalid or unimplemented GPU command type: %u", static_cast<u32>(header.type));
            }
        }
    }

    gpu_end_command_buffer(command_buffer);

    *args->output = command_buffer;
}

void CommandBatcher::submit_commands_job(CommandBatcher* batcher, const i32 count, const CommandBuffer* command_buffers, const FenceHandle& fence)
{
    int command_count = 0;
    for (int cmd = 0; cmd < count; ++cmd)
    {
        command_count += command_buffers[cmd].stream().headers().size();
    }

    auto inputs = FixedArray<GpuCommandHeader>::with_size(command_count, temp_allocator());
    auto outputs = FixedArray<GpuCommandHeader>::with_size(command_count, temp_allocator());

    int offset = 0;
    // Gather the commands into a sortable stream
    for (int cmd = 0; cmd < count; ++cmd)
    {
        auto& stream = command_buffers[cmd].stream();
        memcpy(inputs.data() + offset, stream.headers().data(), sizeof(GpuCommandHeader) * stream.headers().size());
        offset += stream.headers().size();
    }


    // Sort them by header sort key
    radix_sort(inputs.data(), outputs.data(), inputs.size(), [](const GpuCommandHeader& header)
    {
        return header.sort_key;
    });

    // translate N commands per worker - one job per thread
    const auto compile_jobs_count = math::max(outputs.size() / get_job_worker_count(), 1);
    const auto commands_per_job = outputs.size() / compile_jobs_count;
    auto queue_masks = FixedArray<QueueType>::with_size(compile_jobs_count, temp_allocator());

    // get the overall command queue mask for each jobs range of commands
    //  TODO(Jacob): there has got to be a better way, surely...
    for (int header = 0; header < outputs.size(); ++header)
    {
        queue_masks[header / commands_per_job] |= outputs[header].queue_type;
    }

    auto gpu_command_buffers = FixedArray<GpuCommandBuffer*>::with_size(compile_jobs_count, temp_allocator());

    JobGroup record_wait_handle;
    auto compile_args = FixedArray<CompileCommandsArgs>::with_size(compile_jobs_count, temp_allocator());

    for (int i = 0; i < compile_jobs_count; ++i)
    {
        compile_args[i].batcher = batcher;
        compile_args[i].fence = fence;
        compile_args[i].commands_count = outputs.size();
        compile_args[i].commands = outputs.data();
        compile_args[i].queue = queue_masks[i];
        compile_args[i].output = &gpu_command_buffers[i];

        auto job = create_job(compile_commands_job, &compile_args[i]);
        job_schedule(&record_wait_handle, job);
    }

    job_wait(&record_wait_handle);

    SubmitInfo info{};
    info.fence = fence;
    info.command_buffer_count = gpu_command_buffers.size();
    info.command_buffers = gpu_command_buffers.data();

    JobGroup submit_wait_handle;
    gpu_submit(&submit_wait_handle, batcher->device(), info);
    job_wait(&submit_wait_handle);
}

/*
 ************************************
 *
 * Command pools
 *
 ************************************
 */
CommandBatcher::LocalCommandPool::LocalCommandPool(const DeviceHandle& owning_device)
    : device(owning_device)
{
    CommandPoolCreateInfo create_info{};
    create_info.used_queues_hint = QueueType::all;
    create_info.pool_hint = CommandPoolHint::allow_individual_reset;
    handle = gpu_create_command_pool(device, create_info);
}

CommandBatcher::LocalCommandPool::~LocalCommandPool()
{
    for (auto& frame : command_buffers)
    {
        for (auto& cmd : frame)
        {
            gpu_destroy_command_buffer(cmd.cmd);
        }
    }

    gpu_destroy_command_pool(device, handle);
}

// FIXME(Jacob): this leaks a command buffer about once every 10 or so frames
GpuCommandBuffer* CommandBatcher::LocalCommandPool::obtain(const QueueType required_queue_type, const FenceHandle& fence)
{
    auto& frame = command_buffers[gpu_get_current_frame(device)];

    for (auto& cmd : frame)
    {
        if (cmd.pool_version != version)
        {
            cmd.pool_version = version;
            cmd.in_use = false;
        }
        
        if (cmd.in_use)
        {
            continue;
        }

        if (cmd.queue_type != required_queue_type)
        {
            continue;
        }

        cmd.fence = fence;
        cmd.in_use = true;
        cmd.pool_version = version;
        gpu_reset_command_buffer(cmd.cmd);
        return cmd.cmd;
    }

    frame.push_back_no_construct();

    auto& cmd = frame.back();
    cmd.in_use = true;
    cmd.pool_version = version;
    cmd.fence = fence;
    cmd.queue_type = required_queue_type;
    cmd.cmd = gpu_create_command_buffer(device, handle, required_queue_type);

    return cmd.cmd;
}

/*
 ************************************
 *
 * Command context - implementation
 *
 ************************************
 */
CommandBatcher::CommandBatcher(const DeviceHandle& device)
    : device_(device)
{
    per_worker_pools_.resize(get_job_worker_count());

    for (auto& pool : per_worker_pools_)
    {
        new (&pool) LocalCommandPool(device);
    }
}

CommandBatcher::~CommandBatcher()
{
    BEE_ASSERT_F(
        all_jobs_.pending_count() <= 0,
        "Destroyed a command context with pending jobs. You can call `wait_all` to wait on all pending jobs "
        "before the destructor is called"
    );

    for (auto& frame : fences_)
    {
        if (frame.empty())
        {
            continue;
        }

        gpu_wait_for_fences(device_, static_cast<u32>(frame.size()), frame.data(), FenceWaitType::all);

        for (auto& fence : frame)
        {
            gpu_destroy_fence(device_, fence);
        }
    }
}

FenceHandle CommandBatcher::submit_batch(JobGroup* wait_handle, const i32 count, const CommandBuffer* command_buffers)
{
    all_jobs_.add_dependency(wait_handle);

    auto frame = gpu_get_current_frame(device_);

    if (last_submit_frame_ == -1 || last_submit_frame_ != frame)
    {
        last_submit_frame_ = frame;
        next_fence_ = 0;
    }

    if (next_fence_ >= fences_[frame].size())
    {
        fences_[frame].push_back(gpu_create_fence(device_, FenceState::signaled));
    }

    const auto fence = fences_[frame][next_fence_++];

    gpu_wait_for_fence(device_, fence);

    if (gpu_get_fence_state(device_, fence) == FenceState::signaled)
    {
        gpu_reset_fence(device_, fence);
    }

    auto job = create_job(submit_commands_job, this, count, command_buffers, fence);
    job_schedule(wait_handle, job);

    return fence;
}

FenceHandle CommandBatcher::submit_batch(const i32 count, const CommandBuffer* command_buffers)
{
    JobGroup wait_handle;
    const auto fence = submit_batch(&wait_handle, count, command_buffers);
    job_wait(&wait_handle);
    return fence;
}


void CommandBatcher::wait_all()
{
    job_wait(&all_jobs_);
}



} // namespace bee