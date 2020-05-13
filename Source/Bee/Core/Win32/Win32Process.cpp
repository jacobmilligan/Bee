/*
 *  Win32Process.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/Logger.hpp"

#define BEE_MINWINDOWS_ENABLE_USER
#define BEE_MINWINDOWS_ENABLE_WINDOWING
#include "Bee/Core/Win32/MinWindows.h"


#define BEE_ASSERT_PROCESS(process)             \
    BEE_BEGIN_MACRO_BLOCK                       \
        BEE_ASSERT(process.process != nullptr); \
        BEE_ASSERT(process.pid > -1);           \
    BEE_END_MACRO_BLOCK



namespace bee {


bool create_process(const CreateProcessInfo& info, const Path& working_directory)
{
    BEE_ASSERT(info.handle != nullptr);

    SECURITY_ATTRIBUTES attributes{}; // reused for thread attributes (same settings)
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.lpSecurityDescriptor = nullptr;
    attributes.bInheritHandle = true; // make the returned handle inherited in the child process

    const auto create_pipes = (info.flags & CreateProcessFlags::create_read_write_pipes) != CreateProcessFlags::none;
    if (create_pipes)
    {
        if (BEE_FAIL_F(CreatePipe(&info.handle->read_pipe, &info.handle->write_pipe, &attributes, 0) != 0, "Failed to create child process read/write pipes: %s", win32_get_last_error_string()))
        {
            return false;
        }

        if (BEE_FAIL_F(SetHandleInformation(info.handle->read_pipe, HANDLE_FLAG_INHERIT, 0) != 0, "Failed to redirect handle read pipe: %s", win32_get_last_error_string()))
        {
            return false;
        }
    }

    const auto high_priority = (info.flags & CreateProcessFlags::priority_high) != CreateProcessFlags::none;
    const auto low_priority = (info.flags & CreateProcessFlags::priority_low) != CreateProcessFlags::none;
    const auto invalid_priority_setting = high_priority && low_priority;

    if (BEE_FAIL_F(!invalid_priority_setting, "Cannot exec process: invalid priority setting: %u", info.flags))
    {
        return false;
    }

    DWORD creation_flags = NORMAL_PRIORITY_CLASS;
    if (high_priority)
    {
        creation_flags |= ABOVE_NORMAL_PRIORITY_CLASS;
    }
    if (low_priority)
    {
        creation_flags |= BELOW_NORMAL_PRIORITY_CLASS;
    }

    if ((info.flags & CreateProcessFlags::create_detached) != CreateProcessFlags::none)
    {
        creation_flags |= DETACHED_PROCESS;
    }

    STARTUPINFO startup_info{};
    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.lpReserved = nullptr;
    startup_info.lpDesktop = nullptr;
    startup_info.lpTitle = nullptr;
    startup_info.dwX = static_cast<DWORD>(CW_USEDEFAULT);
    startup_info.dwY = static_cast<DWORD>(CW_USEDEFAULT);
    startup_info.dwXSize = static_cast<DWORD>(CW_USEDEFAULT);
    startup_info.dwYSize = static_cast<DWORD>(CW_USEDEFAULT);
    startup_info.dwXCountChars = 0;
    startup_info.dwYCountChars = 0;
    startup_info.dwFillAttribute = 0;
    startup_info.dwFlags = !create_pipes ? 0 : STARTF_USESTDHANDLES;
    startup_info.wShowWindow = 0;

    if ((info.flags & CreateProcessFlags::create_hidden) != CreateProcessFlags::none)
    {
        startup_info.dwFlags |= STARTF_USESHOWWINDOW;
        startup_info.wShowWindow = SW_HIDE;
    }

    startup_info.cbReserved2 = 0;
    startup_info.lpReserved2 = nullptr;
    startup_info.hStdInput = !create_pipes ? nullptr : info.handle->read_pipe;
    startup_info.hStdOutput = !create_pipes ? nullptr : info.handle->write_pipe;
    startup_info.hStdError = !create_pipes ? nullptr : info.handle->write_pipe;

    String temp_args(info.command_line == nullptr ? "" : info.command_line, temp_allocator());

    PROCESS_INFORMATION proc_info{};
    const auto result = CreateProcess(
        info.program,
        temp_args.data(),
        &attributes,
        &attributes,
        TRUE,
        creation_flags,
        nullptr,
        working_directory.c_str(),
        &startup_info,
        &proc_info
    );

    info.handle->process = nullptr;
    info.handle->pid = -1;

    if (BEE_FAIL_F(result != 0, "Unable to create process from application \"%s\": %s", info.program, win32_get_last_error_string()))
    {
        return false;
    }

    info.handle->process = proc_info.hProcess;
    info.handle->pid = proc_info.dwProcessId;

     // don't need to keep a handle open to the child processes primary thread, just the process handle
    CloseHandle(proc_info.hThread);
    return true;
}

void destroy_process(const ProcessHandle& process)
{
    BEE_ASSERT_PROCESS(process);
    if (process.read_pipe != nullptr)
    {
        CloseHandle(process.read_pipe);
    }
    if (process.write_pipe != nullptr)
    {
        CloseHandle(process.write_pipe);
    }
    CloseHandle(process.process);
}

i32 get_process_exit_code(const ProcessHandle& process)
{
    BEE_ASSERT_PROCESS(process);

    DWORD return_code = 0;
    if (BEE_FAIL_F(GetExitCodeProcess(process.process, &return_code) != 0, "Failed to get exit code: %s", win32_get_last_error_string()))
    {
        return -1;
    }

    return sign_cast<i32>(return_code);
}

bool is_process_active(const ProcessHandle& process)
{
    return get_process_exit_code(process) == STILL_ACTIVE;
}

void wait_for_process(const ProcessHandle& process)
{
    BEE_ASSERT_PROCESS(process);
    WaitForSingleObject(process.process, INFINITE);
}

String read_process(const ProcessHandle& process)
{
    BEE_ASSERT_PROCESS(process);
    BEE_ASSERT(process.read_pipe != nullptr);

    DWORD bytes_available{};
    auto success = PeekNamedPipe(process.read_pipe, nullptr, 0, nullptr, &bytes_available, nullptr);
    if (success == 0 || bytes_available <= 0)
    {
        return "";
    }

    String result(sign_cast<i32>(bytes_available), '\0');

    DWORD bytes_read = 0;
    success = ReadFile(process.read_pipe, result.data(), bytes_available, &bytes_read, nullptr);
    if (BEE_FAIL_F(success != 0, "Failed to read from process: %s", win32_get_last_error_string()))
    {
        return "";
    }

    return result;
}

i32 write_process(const ProcessHandle& process, const StringView& data)
{
    BEE_ASSERT_PROCESS(process);
    BEE_ASSERT(process.write_pipe != nullptr);

    DWORD bytes_written = 0;
    const auto success = WriteFile(process.write_pipe, data.data(), sign_cast<u32>(data.size()), &bytes_written, nullptr);
    if (BEE_FAIL_F(success != 0, "Failed to write to process: %s", win32_get_last_error_string()))
    {
        return -1;
    }

    return sign_cast<i32>(bytes_written);
}

bool get_environment_variable(const char* variable, String* dst)
{
    const auto size = GetEnvironmentVariable(variable, nullptr, 0);

    if (size == 0)
    {
        return false;
    }

    dst->resize(size);
    GetEnvironmentVariable(variable, dst->data(), dst->size());
    return true;
}

i32 get_environment_variable(const char* variable, char* buffer, const i32 buffer_length)
{
    const auto size = GetEnvironmentVariable(variable, buffer, buffer_length);
    return size == 0 ? -1 : size;
}


} // namespace bee
