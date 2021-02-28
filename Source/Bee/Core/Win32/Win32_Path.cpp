/*
 *  Win32Path.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Path.hpp"
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Filesystem.hpp"

namespace bee {


PathView executable_path()
{
    static thread_local StaticArray<char, 4096> exe_path;

    if (exe_path.empty())
    {
        StaticArray<wchar_t, 4096> u16s;
        u16s.size = ::GetModuleFileNameW(nullptr, u16s.data, u16s.capacity);
        BEE_ASSERT_F(!u16s.empty(), "Failed to get executable path: %s", win32_get_last_error_string());

        exe_path.size = str::from_wchar(exe_path.data, exe_path.capacity, u16s.data, u16s.size);
        BEE_ASSERT(!exe_path.empty());
    }

    return StringView(exe_path.data, exe_path.size);
}

PathView current_working_directory()
{
    static thread_local StaticArray<wchar_t, 4096> u16s;
    static thread_local StaticArray<char, 4096> path;

    u16s.size = ::GetCurrentDirectoryW(u16s.capacity, u16s.data);

    if (BEE_FAIL_F(!u16s.empty(), "Failed to get current working directory: %s", win32_get_last_error_string()))
    {
        return PathView{};
    }

    path.size = str::from_wchar(path.data, path.capacity, u16s.data, u16s.size);
    BEE_ASSERT(!path.empty());

    return StringView(path.data, path.size);
}

Path& Path::normalize()
{
    static constexpr DWORD buf_len = 4096;
    TCHAR buf[buf_len];
    auto len = GetFullPathName(data_.c_str(), buf_len, buf, nullptr);
    if (BEE_CHECK_F(len > 0, "Failed to normalize path %s: %s", data_.c_str(), win32_get_last_error_string()))
    {
        data_ = buf;
    }
    return *this;
}

Path Path::get_normalized(Allocator* allocator) const
{
    Path normalized_path(view(), allocator);
    normalized_path.normalize();
    return BEE_MOVE(normalized_path);
}

bool PathView::exists() const
{
    auto buf = str::to_wchar<MAX_PATH>(data_);
    auto attrs = GetFileAttributesW(buf.data);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        const auto error = GetLastError();
        switch (error)
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_NOT_READY:
            case ERROR_INVALID_DRIVE:
            {
                return false;
            }
            default:
            {
                break;
            }
        }

        return BEE_CHECK_F(
            attrs != INVALID_FILE_ATTRIBUTES,
            "Path::exists failed for path at '%" BEE_PRIsv "` with error: %s", BEE_FMT_SV(data_), win32_get_last_error_string()
        );
    }

    return true;
}

bool PathView::has_root_name() const
{
    // see: https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats
    // TODO(Jacob): Ignore UNC paths for now
    return data_.size() >= 3 && isalpha(data_[0]) && data_[1] == ':';
}

PathView PathView::root_name() const
{
    if (!has_root_name())
    {
        return StringView{};
    }
    return str::substring(data_, 0, 3);
}

bool PathView::is_absolute() const
{
    // see: https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats
    // if the path has a drive and a directory name
    if (!has_root_path())
    {
        return false;
    }

    return fs::is_file(*this);
}


} // namespace bee
