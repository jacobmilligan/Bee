/*
 *  Command.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"


namespace bee {


/*
 ********************************************************
 *
 * # CPU Command buffer API
 *
 ********************************************************
 */
BEE_VERSIONED_HANDLE_32(CommandBatchHandle);


struct CommandChunk
{
    CommandChunk*   next { nullptr };
    size_t          capacity { 0 };
    size_t          size { 0 };
    u8*             data { nullptr };
};

class BEE_RUNTIME_API CommandAllocator
{
public:
    CommandAllocator() = default;

    explicit CommandAllocator(const size_t chunk_size);

    ~CommandAllocator();

    CommandChunk* allocate();

    void deallocate(CommandChunk* chunk);

    void trim();

    void free_all_chunks();
private:
    struct Header
    {
        Header* next { nullptr };
        Header* previous { nullptr };
    };

    size_t          chunk_size_ { 0 };
    Header*         chunks_ { nullptr };
    CommandChunk*   free_chunks_ { nullptr };
};


class BEE_RUNTIME_API CommandStream
{
public:
    CommandStream() = default;

    explicit CommandStream(CommandAllocator* allocator);

    ~CommandStream();

    void reset(const CommandStreamReset reset_type = CommandStreamReset::none);

    template <typename CmdType>
    CmdType* push_command()
    {
        auto cmd = static_cast<CmdType*>(allocate_command(CmdType::command_type, CmdType::queue_type, sizeof(CmdType)));

        new (cmd) CmdType{};

        return cmd;
    }

    template <typename CmdType, typename DynamicDataType>
    CmdType* push_command_with_dynamic_data(DynamicDataType** data)
    {
        BEE_ASSERT(data != nullptr);

        auto alloc = static_cast<u8*>(allocate_command(CmdType::command_type, CmdType::queue_type, sizeof(CmdType) + sizeof(DynamicDataType)));
        auto cmd = reinterpret_cast<CmdType*>(alloc);
        *data = reinterpret_cast<DynamicDataType*>(alloc + sizeof(CmdType));

        new (cmd) CmdType{};

        return cmd;
    }


    inline DynamicArray<GpuCommandHeader>& headers()
    {
        return headers_;
    }

    inline const DynamicArray<GpuCommandHeader>& headers() const
    {
        return headers_;
    }

private:
    DynamicArray<GpuCommandHeader>  headers_;
    CommandAllocator*               allocator_ { nullptr };
    CommandChunk*                   first_chunk_ { nullptr };
    CommandChunk*                   last_chunk_ { nullptr };
    CommandChunk*                   current_chunk_ { nullptr };

    void push_chunk();

    void free_chunks() noexcept;

    void* allocate_command(const GpuCommandType type, const QueueType queue_type, const size_t size);
};


class BEE_RUNTIME_API CommandBuffer
{
public:
    CommandBuffer() = default;

    explicit CommandBuffer(CommandAllocator* allocator);

    ~CommandBuffer();

    void begin_render_pass(
        const RenderPassHandle&     pass,
        const u32                   attachment_count,
        const TextureViewHandle*    attachments,
        const RenderRect&           render_area,
        const u32                   clear_value_count,
        const ClearValue*           clear_values
    );

    void end_render_pass();

    void bind_pipeline_state(const PipelineStateHandle& pipeline);

    void bind_vertex_buffer(const BufferHandle& buffer, const u32 binding, const u32 offset);

    void bind_vertex_buffers(
        const u32           first_binding,
        const u32           count,
        const BufferHandle* buffers,
        const u32*          offsets
    );

    void bind_index_buffer(const BufferHandle& buffer, const u32 offset, const IndexFormat index_format);

    void copy_buffer(
        const BufferHandle& src,
        const i32           src_offset,
        const BufferHandle& dst,
        const i32           dst_offset,
        const i32           size
    );

    void draw(const u32 vertex_count, const u32 instance_count, const u32 first_vertex, const u32 first_instance);

    void draw_indexed(
        const u32 index_count,
        const u32 instance_count,
        const u32 vertex_offset,
        const u32 first_index,
        const u32 first_instance
    );

    void set_viewport(const Viewport& viewport);

    void set_scissor(const RenderRect& scissor);

    void transition_resources(const u32 count, const GpuTransition* transitions);

    void end();

    inline CommandStream& stream()
    {
        return stream_;
    }

    inline const CommandStream& stream() const
    {
        return stream_;
    }

    inline QueueType queue_mask() const
    {
        return queue_mask_;
    }

    inline void reset()
    {
        stream_.reset();
    }

    inline i32 count()
    {
        return stream_.headers().size();
    }

private:
    CommandStream           stream_;
    DrawItem                current_draw_;
    bool                    is_in_pass_ { false };
    QueueType               queue_mask_ { QueueType::none };

    template <typename T>
    T* push_command()
    {
        auto cmd = stream_.push_command<T>();
        queue_mask_ |= T::queue_type;
        return cmd;
    }

    template <typename T, typename DynamicDataType>
    T* push_command_with_dynamic_data(DynamicDataType** data)
    {
        auto cmd = stream_.push_command_with_dynamic_data<T>(data);
        queue_mask_ |= T::queue_type;
        return cmd;
    }
};

class BEE_RUNTIME_API CommandBatcher
{
public:
    CommandBatcher() = default;

    explicit CommandBatcher(const DeviceHandle& device);

    ~CommandBatcher();

    FenceHandle submit_batch(JobGroup* wait_handle, const i32 count, const CommandBuffer* command_buffers);

    FenceHandle submit_batch(const i32 count, const CommandBuffer* command_buffers);

    void wait_all();

    inline DeviceHandle device() const
    {
        return device_;
    }
private:

    struct PooledCommandBuffer
    {
        bool                in_use { false };
        u32                 pool_version { 0 };
        FenceHandle         fence;
        QueueType           queue_type { QueueType::none };
        GpuCommandBuffer*   cmd { nullptr };
    };

    struct LocalCommandPool
    {
        u32                                 version { 0 };
        DeviceHandle                        device;
        CommandPoolHandle                   handle;
        DynamicArray<PooledCommandBuffer>   command_buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

        LocalCommandPool() = default;

        explicit LocalCommandPool(const DeviceHandle& owning_device);

        ~LocalCommandPool();

        GpuCommandBuffer* obtain(const QueueType required_queue_type, const FenceHandle& fence);
    };

    struct CompileCommandsArgs
    {
        CommandBatcher*         batcher {nullptr };
        FenceHandle             fence;
        QueueType               queue { QueueType::none };
        i32                     commands_count { 0 };
        const GpuCommandHeader* commands {nullptr };
        GpuCommandBuffer**      output {nullptr };
    };

    static void compile_commands_job(CompileCommandsArgs* args);

    static void submit_commands_job(CommandBatcher* batcher, const i32 count, const CommandBuffer* command_buffers, const FenceHandle& fence);

    DeviceHandle                    device_;
    JobGroup                        all_jobs_;
    FixedArray<LocalCommandPool>    per_worker_pools_;
    i32                             last_submit_frame_ { -1 };
    i32                             next_fence_ { 0 };
    DynamicArray<FenceHandle>       fences_[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
};


} // namespace bee
