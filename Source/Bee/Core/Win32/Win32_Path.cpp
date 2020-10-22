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


Path Path::executable_path(Allocator* allocator)
{
    TCHAR file_name[4096];
    GetModuleFileNameA(nullptr, file_name, 4096);
    return Path(StringView(file_name), allocator);
}

Path Path::current_working_directory(Allocator* allocator)
{
    char buf[limits::max<u16>()];
    const auto num_chars_written = GetCurrentDirectoryA(limits::max<u16>(), buf);
    if (!BEE_CHECK(num_chars_written != 0))
    {
        return Path();
    }

    return Path(StringView(buf), allocator);
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
    return std::move(normalized_path);
}

bool Path::exists() const
{
    auto attrs = GetFileAttributesA(data_.c_str());
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
            "Path::exists failed for path at '%s' with error: %s", data_.c_str(), win32_get_last_error_string()
        );
    }

    return true;
}

bool path_has_root_name(const StringView& path)
{
    // see: https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats
    // TODO(Jacob): Ignore UNC paths for now
    return path.size() >= 3 && isalpha(path[0]) && path[1] == ':';
}

StringView get_root_name(const StringView& path)
{
    if (!path_has_root_name(path))
    {
        return StringView{};
    }
    return StringView(path.data(), 3);
}

StringView Path::root_name() const
{
    return get_root_name(data_.view());
}

bool Path::is_absolute(const StringView& path) const
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
