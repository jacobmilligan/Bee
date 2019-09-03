/*
 *  Win32Filesystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Containers/HandleTable.hpp"

#define BEE_MINWINDOWS_ENABLE_SHELLAPI
#include "Bee/Core/Win32/MinWindows.h"

#include <ShlObj.h>


namespace bee {
namespace fs {


struct DirectoryEntry
{
    WIN32_FIND_DATA find_data;
    HANDLE          handle { nullptr };
};

// TODO(Jacob): add a BEE_CONFIG #define for the size of the table?
//  32 seems pretty fair for doing recursive directory iterating for the moment
thread_local static HandleTable<32, DirectoryEntryHandle, DirectoryEntry> thread_local_entries;

/*
 *****************************************
 *
 * DirectoryIterator - implementation
 *
 *****************************************
 */
DirectoryIterator::~DirectoryIterator()
{
    destroy();
}

void DirectoryIterator::destroy()
{
    if (thread_local_entries.contains(current_handle_))
    {
        FindClose(thread_local_entries[current_handle_]->handle);
        thread_local_entries.destroy(current_handle_);
    }

    current_handle_ = DirectoryEntryHandle{};
}

void DirectoryIterator::init()
{
    if (dir_.empty())
    {
        return;
    }

    BEE_ASSERT(!current_handle_.is_valid());

    dir_.append("*");
    DirectoryEntry entry{};
    entry.handle = FindFirstFile(dir_.c_str(), &entry.find_data);
    current_handle_ = thread_local_entries.create(entry);

    if (BEE_FAIL_F(current_handle_.is_valid(), "Failed to find file in directory: %s: %s", dir_.c_str(), win32_get_last_error_string()))
    {
        return;
    }

    next();
}

void DirectoryIterator::next()
{
    if (!thread_local_entries.contains(current_handle_))
    {
        return;
    }

    auto entry = thread_local_entries[current_handle_];
    StringView next_filename{};
    do
    {
        if (FindNextFile(entry->handle, &entry->find_data) == 0)
        {
            destroy();
            return;
        }

        next_filename = StringView(entry->find_data.cFileName, str::length(entry->find_data.cFileName));
    } while (next_filename == "." || next_filename == "..");

    dir_.replace_filename(next_filename);
}

DirectoryIterator::reference DirectoryIterator::operator*() const
{
    return dir_;
}

DirectoryIterator::pointer DirectoryIterator::operator->() const
{
    return &dir_;
}

bool DirectoryIterator::operator==(const bee::fs::DirectoryIterator& other) const
{
    return current_handle_ == other.current_handle_;
}

/*
 *****************************************
 *
 * Filesystem functions - implementation
 *
 *****************************************
 */
bool is_dir(const Path& path)
{
    const auto file_attributes = GetFileAttributes(path.c_str());
    return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool is_file(const Path& path)
{
    return !is_dir(path);
}

bool mkdir(const Path& directory_path)
{
    const auto result = CreateDirectory(directory_path.c_str(), nullptr);
    return BEE_CHECK_F(result != 0, "Unable to make directory at path: %s: %s", directory_path.c_str(), win32_get_last_error_string());
}

bool native_rmdir_non_recursive(const Path& directory_path)
{
    const auto result = RemoveDirectory(directory_path.c_str());
    return BEE_CHECK_F(result != 0, "Unable to destroy directory at path: %s: %s", directory_path.c_str(), win32_get_last_error_string());
}

bool remove(const Path& filepath)
{
    const auto result = DeleteFile(filepath.c_str());
    return BEE_CHECK_F(result != 0, "Unable to destroy file at path: %s: %s", filepath.c_str(), win32_get_last_error_string());
}

bool copy(const Path& src_filepath, const Path& dst_filepath, bool overwrite)
{
    const auto result = CopyFile(src_filepath.c_str(), dst_filepath.c_str(), !overwrite);
    return BEE_CHECK_F(result != 0, "Unable to copy file %s to destination %s: %s", src_filepath.c_str(), dst_filepath.c_str(), win32_get_last_error_string());
}

/*
 *********************************
 *
 * Local data - implementation
 *
 *********************************
 */
Path user_local_appdata_path()
{
    LPWSTR path_str = nullptr;
    const auto result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_str);
    if (BEE_FAIL_F(result == S_OK, "Couldn't retrieve local app data folder"))
    {
        return Path();
    }

    const Path appdata = str::from_wchar(path_str).view();
    CoTaskMemFree(path_str);

    return appdata;
}


} // namespace fs
} // namespace bee
