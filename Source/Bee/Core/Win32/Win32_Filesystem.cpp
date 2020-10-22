/*
 *  Win32Filesystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Containers/HandleTable.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Logger.hpp"

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

    auto* entry = thread_local_entries[current_handle_];
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
 *************************************
 *
 * DirectoryWatcher - implementation
 *
 *************************************
 */
struct WatchedDirectory
{
    static constexpr auto notify_buffer_capacity = 4096;

    bool        scheduled_for_removal { false };
    i32         index { -1 };
    HANDLE      directory { nullptr };
    u8          notify_buffer[notify_buffer_capacity];
    DWORD       buffer_size { 0 };
    OVERLAPPED  overlapped;

    ~WatchedDirectory()
    {
        if (directory != nullptr)
        {
            CloseHandle(directory);
        }

        directory = nullptr;
    }
};

static constexpr auto notify_flags = FILE_NOTIFY_CHANGE_LAST_WRITE
    | FILE_NOTIFY_CHANGE_DIR_NAME
    | FILE_NOTIFY_CHANGE_ATTRIBUTES
    | FILE_NOTIFY_CHANGE_FILE_NAME;


// ctr/dtr Must be in this TU to ensure opaque struct WatchedDirectory can be compiled
DirectoryWatcher::DirectoryWatcher(const bool recursive)
    : recursive_(recursive)
{

}

DirectoryWatcher::~DirectoryWatcher()
{
    if (is_running_.load(std::memory_order_relaxed))
    {
        stop();
    }
}

void DirectoryWatcher::init(const ThreadCreateInfo &thread_info)
{
    thread_ = Thread(thread_info, watch_loop, this);
}

void DirectoryWatcher::remove_directory(const Path &path)
{
    scoped_lock_t lock(mutex_);

    const auto index = find_entry(path);

    if (index < 0)
    {
        log_error("Directory at path %s is not being watched", path.c_str());
        return;
    }

    entries_[index]->scheduled_for_removal = true;
}

void DirectoryWatcher::finalize_removal(const i32 index)
{
    scoped_lock_t lock(mutex_);

    entries_.erase(index);
    watched_paths_.erase(index);

    for (int i = index; i < entries_.size(); ++i)
    {
        entries_[i]->index = i;
    }
}

void DirectoryWatcher::stop()
{
    if (!is_running_.load(std::memory_order_relaxed))
    {
        log_warning("DirectoryWatcher is already stopped");
        return;
    }

    auto* completion_port = static_cast<HANDLE>(native_);

    is_running_.store(false);

    ::PostQueuedCompletionStatus(completion_port, 0, 0, nullptr);

    thread_.join();
    entries_.clear();

    CloseHandle(completion_port);
}

bool DirectoryWatcher::add_directory(const Path& path)
{
    if (!is_dir(path))
    {
        log_error("%s is not a directory", path.c_str());
        return false;
    }

    scoped_lock_t lock(mutex_);

    const auto existing_index = find_entry(path);

    if (existing_index >= 0)
    {
        entries_[existing_index]->scheduled_for_removal = false;
        return true;
    }

    auto entry = make_unique<WatchedDirectory>(system_allocator());
    memset(&entry->overlapped, 0, sizeof(OVERLAPPED));

    entry->directory = ::CreateFile(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (entry->directory == INVALID_HANDLE_VALUE)
    {
        log_error("Failed to open directory: %s", win32_get_last_error_string());
        return false;
    }

    /*
     * There are basically 4 ways to do directory watching with ReadDirectoryChangesW:
     * - blocking synchronous
     * - GetOverlappedResult: requires a separate event object for each dir (i.e. for WaitOnSingleObject)
     * - Completion routines: async function callbacks on completion of the directory operation
     * - IO completion ports: kind of like using a socket
     *
     * Here, we create a single completion port but add a file handle for each watched directory to the same port
     */
    native_ = ::CreateIoCompletionPort(entry->directory, static_cast<HANDLE>(native_), reinterpret_cast<ULONG_PTR>(entry.get()), 0);
    if (native_ == nullptr)
    {
        log_error("Failed to create IO completion port: %s", win32_get_last_error_string());
        return EXIT_FAILURE;
    }

    ::ReadDirectoryChangesW(
        entry->directory,
        entry->notify_buffer,
        WatchedDirectory::notify_buffer_capacity,
        recursive_ ? TRUE : FALSE,
        notify_flags,
        &entry->buffer_size,
        &entry->overlapped,
        nullptr
    );

    if (find_entry(path) >= 0)
    {
        log_error("Directory %s is already being watched", path.c_str());
        return false;
    }

    entry->index = entries_.size();
    entries_.emplace_back(std::move(entry));
    watched_paths_.push_back(path);
    start_thread_cv_.notify_all();

    return true;
}

void DirectoryWatcher::watch_loop(DirectoryWatcher* watcher)
{
    String path_string;
    FileAction action = FileAction::none;

    // We need to wait here until at least one completion port has been created or GetQueuedCompletionStatus will fail
    if (watcher->watched_directories().empty())
    {
        scoped_lock_t lock(watcher->mutex_);
        watcher->start_thread_cv_.wait(lock);
    }

    while (watcher->is_running_.load(std::memory_order_relaxed))
    {
        DWORD bytes_transferred = 0;
        OVERLAPPED* overlapped = nullptr;
        ULONG_PTR completion_key = NULL;

        auto* completion_port = static_cast<HANDLE>(watcher->native_);

        // Don't do anything if we failed to get completion status - waits until something happens on the port
        if (::GetQueuedCompletionStatus(completion_port, &bytes_transferred, &completion_key, &overlapped, INFINITE) == FALSE)
        {
            log_error("Win32 IO completion port failure: %s", win32_get_last_error_string());
            continue;
        }

        auto* entry = reinterpret_cast<WatchedDirectory*>(completion_key);

        if (entry == nullptr)
        {
            continue;
        }

        if (entry->scheduled_for_removal)
        {
            watcher->finalize_removal(entry->index);
            break;
        }

        if (!watcher->is_running_.load(std::memory_order_relaxed))
        {
            break;
        }

        auto* notify_buffer = entry->notify_buffer;
        FILE_NOTIFY_INFORMATION* notify_info = nullptr;
        int offset = 0;

        if (!watcher->is_suspended_.load(std::memory_order_relaxed))
        {
            scoped_lock_t lock(watcher->mutex_);

            do
            {
                // ignore spurious completions
                if (bytes_transferred == 0)
                {
                    break;
                }

                // Get the next notify info event struct in the buffer
                notify_buffer = notify_buffer + offset;
                notify_info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(notify_buffer);

                // Get the path of the file that triggered the event
                path_string.clear();
                str::from_wchar(&path_string, notify_info->FileName, notify_info->FileNameLength);

                /*
                 * We count rename events as added/removed events. Old name is counted as removing a file and new name
                 * is counted as adding a file
                 */
                switch (notify_info->Action)
                {
                    case FILE_ACTION_RENAMED_NEW_NAME:
                    case FILE_ACTION_ADDED:
                    {
                        action = FileAction::added;
                        break;
                    }
                    case FILE_ACTION_RENAMED_OLD_NAME:
                    case FILE_ACTION_REMOVED:
                    {
                        action = FileAction::removed;
                        break;
                    }
                    case FILE_ACTION_MODIFIED:
                    {
                        action = FileAction::modified;
                        break;
                    }
                    default:
                    {
                        action = FileAction::none;
                        break;
                    }
                }

                // Only add the event if we support it
                if (action != FileAction::none)
                {
                    watcher->add_event(action, path_string.view(), entry->index);
                }

                // get the byte offset from the current record to the next one
                offset = notify_info->NextEntryOffset;

            } while (offset > 0);
        }

        memset(notify_buffer, 0, WatchedDirectory::notify_buffer_capacity);
        memset(&entry->overlapped, 0, sizeof(OVERLAPPED));

        // trigger another directory change read to wait on
        ::ReadDirectoryChangesW(
            entry->directory,
            entry->notify_buffer,
            WatchedDirectory::notify_buffer_capacity,
            watcher->recursive_ ? TRUE : FALSE,
            notify_flags,
            &entry->buffer_size,
            &entry->overlapped,
            nullptr
        );
    }
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

u64 last_modified(const Path& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    const auto success = GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &data);
    if (success == 0)
    {
        log_error("Failed to get last modified time: %s", win32_get_last_error_string());
        return 0;
    }

    const auto& filetime = data.ftLastWriteTime;
    return (static_cast<u64>(filetime.dwHighDateTime) << 32) + static_cast<u64>(filetime.dwLowDateTime);
}

bool mkdir(const Path& directory_path, const bool recursive)
{
    if (recursive)
    {
        const auto parent = directory_path.parent_path(temp_allocator());
        if (!parent.exists())
        {
            mkdir(parent, recursive);
        }
    }

    const auto result = CreateDirectory(directory_path.c_str(), nullptr);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to make directory at path: %s: %s", directory_path.c_str(), win32_get_last_error_string());
    return false;
}

bool native_rmdir_non_recursive(const Path& directory_path)
{
    const auto result = RemoveDirectory(directory_path.c_str());

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to remove directory at path: %s: %s", directory_path.c_str(), win32_get_last_error_string());
    return false;
}

bool remove(const Path& filepath)
{
    const auto result = DeleteFile(filepath.c_str());

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to remove file at path: %s: %s", filepath.c_str(), win32_get_last_error_string());
    return false;
}

bool move(const Path& current_path, const Path& new_path)
{
    const auto result = MoveFile(current_path.c_str(), new_path.c_str());

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to move file from %s to %s: %s", current_path.c_str(), new_path.c_str(), win32_get_last_error_string());
    return false;
}

bool copy(const Path& src_filepath, const Path& dst_filepath, bool overwrite)
{
    const auto result = CopyFile(src_filepath.c_str(), dst_filepath.c_str(), !overwrite);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to copy file from %s to %s: %s", src_filepath.c_str(), dst_filepath.c_str(), win32_get_last_error_string());
    return false;
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

    Path appdata = str::from_wchar(path_str).view();
    CoTaskMemFree(path_str);

    return appdata;
}

/*
 ******************************************
 *
 * Memory mapped files - implementation
 *
 ******************************************
 */
bool mmap_file_map(MemoryMappedFile* file, const Path& path, const FileAccess access)
{
    const DWORD desired_access = decode_flag(access, FileAccess::read, GENERIC_READ)
                               | decode_flag(access, FileAccess::write, GENERIC_WRITE);

    const DWORD create_disposition = decode_flag(access, FileAccess::read, OPEN_EXISTING)
                                   | decode_flag(access, FileAccess::write, CREATE_NEW);

    file->handles[0] = CreateFile(
        path.c_str(),
        desired_access,
        FILE_SHARE_READ,
        nullptr,
        create_disposition,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file->handles[0] == INVALID_HANDLE_VALUE)
    {
        log_error("Failed to create memory mapped file %s: %s", path.c_str(), win32_get_last_error_string());
        file->handles[0] = nullptr;
        return false;
    }

    const DWORD protect = decode_flag(access, FileAccess::read, PAGE_READONLY)
                        | decode_flag(access, FileAccess::write, PAGE_READWRITE);

    file->handles[1] = CreateFileMapping(
        file->handles[0],
        nullptr,
        protect,
        0, 0,       // 0,0 - maxsize == sizeof file
        nullptr
    );

    if (file->handles[1] == nullptr)
    {
        log_error("Failed to memory map file %s: %s", path.c_str(), win32_get_last_error_string());
        CloseHandle(file->handles[0]);
        return false;
    }

    const DWORD view_access = decode_flag(access, FileAccess::read, FILE_MAP_READ)
                            | decode_flag(access, FileAccess::write, FILE_MAP_WRITE);
    file->data = MapViewOfFile(file->handles[1], view_access, 0, 0, 0);
    if (file->data == nullptr)
    {
        log_error("Failed to map file view %s: %s", path.c_str(), win32_get_last_error_string());
        CloseHandle(file->handles[0]);
        CloseHandle(file->handles[1]);
        return false;
    }

    return true;
}

bool mmap_file_unmap(MemoryMappedFile* file)
{
    if (UnmapViewOfFile(file->data) == FALSE)
    {
        log_error("Failed to unmap file view: %s", win32_get_last_error_string());
        return false;
    }

    CloseHandle(file->handles[0]);
    CloseHandle(file->handles[1]);
    new (file) MemoryMappedFile{};
    return true;
}


} // namespace fs
} // namespace bee
