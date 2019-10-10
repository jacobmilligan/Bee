/*
 *  Process.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Enum.hpp"


namespace bee {



BEE_FLAGS(CreateProcessFlags, u32)
{
    /**
     * Create the new process as a default process
     */
    none                    = 0u,

    /**
     * The new process is detached from its parent process. Depending on the platform, this will determine if the new
     * process shares a console or not with its parent
     */
    create_detached         = 1u << 0u,

    /**
     * Sets the new processes priority to high
     */
    priority_high           = 1u << 1u,

    /**
    * Sets the new processes priority to low
    */
    priority_low            = 1u << 2u,

    /**
     * Creates the new process with its console window hidden
     */
    create_hidden           = 1u << 3u,

    /**
     * Redirects the child processes std in/out to new read/write pipes
     */
    create_read_write_pipes = 1u << 4u
};


struct BEE_CORE_API ProcessHandle : public Noncopyable
{
    ProcessHandle() = default;

    ProcessHandle(ProcessHandle&& other) noexcept
        : pid(other.pid),
          process(other.process)
    {
        other.pid = -1;
        other.process = nullptr;
    }

    ProcessHandle& operator=(ProcessHandle&& other) noexcept
    {
        process = other.process;
        pid = other.pid;
        other.pid = -1;
        other.process = nullptr;
        return *this;
    }

    inline bool is_valid() const
    {
        return process != nullptr;
    }

    i32     pid { -1 };
    void*   process { nullptr };
    void*   write_pipe { nullptr };
    void*   read_pipe { nullptr };
};


struct BEE_CORE_API CreateProcessInfo
{
    CreateProcessFlags  flags { CreateProcessFlags::none };
    ProcessHandle*      handle { nullptr };
    const char*         program { nullptr };
    const char*         command_line { nullptr };
};

BEE_CORE_API bool create_process(const CreateProcessInfo& info, const Path& working_directory = Path::current_working_directory());

BEE_CORE_API void destroy_process(const ProcessHandle& process);

BEE_CORE_API i32 get_process_exit_code(const ProcessHandle& process);

BEE_CORE_API bool is_process_active(const ProcessHandle& process);

BEE_CORE_API void wait_for_process(const ProcessHandle& process);

BEE_CORE_API String read_process(const ProcessHandle& process);

BEE_CORE_API i32 write_process(const ProcessHandle& process, const StringView& data);


} // namespace bee