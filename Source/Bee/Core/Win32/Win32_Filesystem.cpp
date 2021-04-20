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
#include "Bee/Core/Bit.hpp"

#define BEE_MINWINDOWS_ENABLE_SHELLAPI
#include "Bee/Core/Win32/MinWindows.h"

#include <ShlObj.h>


namespace bee {
namespace fs {


struct DirectoryEntry
{
    WIN32_FIND_DATAW    find_data;
    HANDLE              handle { nullptr };
    StaticString<4096>  buffer;
    char                u8s_filename[MAX_PATH + 4]; // + 4 for struct padding
    PathView            root;
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
DirectoryIterator::DirectoryIterator(const PathView& directory_path)
{
    if (directory_path.empty())
    {
        return;
    }

    BEE_ASSERT(!current_handle_.is_valid());

    DirectoryEntry* entry = nullptr;
    current_handle_ = thread_local_entries.create_uninitialized(&entry);

    if (BEE_FAIL_F(current_handle_.is_valid(), "Failed to find file in directory: %" BEE_PRIsv ": %s", BEE_FMT_SV(directory_path), win32_get_last_error_string()))
    {
        return;
    }

    entry->buffer.append(directory_path.string_view()).append(Path::preferred_slash).append('*');

    auto u16s = str::to_wchar<MAX_PATH>(entry->buffer.view());
    entry->buffer.resize(entry->buffer.size() - 2); // remove the '\\*'
    entry->handle = ::FindFirstFileW(u16s.data, &entry->find_data);
    entry->root = directory_path;

    next();
}

void DirectoryIterator::destroy()
{
    if (thread_local_entries.contains(current_handle_))
    {
        ::FindClose(thread_local_entries[current_handle_]->handle);
        thread_local_entries.destroy(current_handle_);
    }

    current_handle_ = DirectoryEntryHandle{};
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
        if (::FindNextFileW(entry->handle, &entry->find_data) == 0)
        {
            destroy();
            return;
        }

        i32 size = str::from_wchar(
            entry->u8s_filename,
            static_array_length(entry->u8s_filename),
            entry->find_data.cFileName,
            ::wcslen(entry->find_data.cFileName)
        );
        next_filename = StringView(entry->u8s_filename, size);
    } while (next_filename == "." || next_filename == "..");

    entry->buffer = entry->root.string_view();

    const char last_char = entry->buffer[entry->buffer.size() - 1];
    if (last_char != Path::preferred_slash && last_char != Path::generic_slash)
    {
        entry->buffer.append(Path::preferred_slash);
    }

    entry->buffer.append(next_filename);

    path_ = entry->buffer.view();
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

    HANDLE      directory { nullptr };
    i32         index { -1 };
    DWORD       buffer_size { 0 };
    u8          notify_buffer[notify_buffer_capacity];
    OVERLAPPED  overlapped;
    bool        scheduled_for_removal { false };
    BEE_PAD(7);

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
    | FILE_NOTIFY_CHANGE_CREATION
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

void DirectoryWatcher::remove_directory(const PathView& path)
{
    scoped_lock_t lock(mutex_);

    const auto index = find_entry(path);

    if (index < 0)
    {
        log_error("Directory at path %" BEE_PRIsv " is not being watched", BEE_FMT_SV(path));
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

bool DirectoryWatcher::add_directory(const PathView& path)
{
    if (!is_dir(path))
    {
        log_error("%" BEE_PRIsv " is not a directory", BEE_FMT_SV(path));
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

    Path pathbuf(path);
    auto u16s = str::to_wchar<1024>(path.string_view());

    entry->directory = ::CreateFileW(
        u16s.data,
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
        log_error("Directory %s is already being watched", pathbuf.c_str());
        return false;
    }

    entry->index = entries_.size();
    entries_.emplace_back(BEE_MOVE(entry));
    watched_paths_.emplace_back(BEE_MOVE(pathbuf));
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
File open_file(const PathView& path, const OpenMode mode)
{
    DWORD dwDesiredAccess = 0
        | decode_flag(mode, OpenMode::read, GENERIC_READ)
        | decode_flag(mode, OpenMode::write, GENERIC_WRITE)
        | decode_flag(mode, OpenMode::append, GENERIC_READ | GENERIC_WRITE);
    DWORD dwShareMode = FILE_SHARE_READ;
    DWORD dwCreationDisposition = 0
        | decode_flag(mode, OpenMode::read, OPEN_EXISTING)
        | decode_flag(mode, OpenMode::write, CREATE_ALWAYS);

    if ((mode & OpenMode::read) != OpenMode::none)
    {
        dwCreationDisposition = OPEN_EXISTING;
    }

    if ((mode & OpenMode::append) != OpenMode::none)
    {
        dwCreationDisposition = CREATE_NEW;
    }
    else if ((mode & OpenMode::write) != OpenMode::none)
    {
        dwCreationDisposition = CREATE_ALWAYS;
    }

    const auto u16s = str::to_wchar<1024>(path.string_view());
    auto* file = ::CreateFileW(
        u16s.data,
        dwDesiredAccess,
        dwShareMode,
        nullptr,
        dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (BEE_FAIL_F(file != INVALID_HANDLE_VALUE, "Failed to open file %" BEE_PRIsv " for read: %s", BEE_FMT_SV(path), win32_get_last_error_string()))
    {
        return File{};
    }

    return { file, mode };
}

void close_file(File* file)
{
    BEE_ASSERT(file->is_valid());

    ::CloseHandle(file->handle);
    file->handle = nullptr;
    file->mode = OpenMode::none;
}

i64 get_size(const File& file)
{
    BEE_ASSERT(file.is_valid());

    LARGE_INTEGER file_size;
    if (BEE_FAIL_F(FALSE != ::GetFileSizeEx(file.handle, &file_size), "Failed to get file size: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    return file_size.QuadPart;
}

i64 tell(const File& file)
{
    LARGE_INTEGER li{};
    li.QuadPart = 0;

    LARGE_INTEGER offset{};
    if (BEE_FAIL_F(FALSE != ::SetFilePointerEx(static_cast<HANDLE>(file.handle), li, &offset, FILE_CURRENT), "Failed to tell file: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    return offset.QuadPart;
}

BEE_TRANSLATION_TABLE_FUNC(seek_origin_to_move_method, io::SeekOrigin, DWORD, 3,
    FILE_BEGIN,     // begin
    FILE_CURRENT,   // current
    FILE_END        // end
);

i64 seek(const File& file, const i64 offset, const io::SeekOrigin origin)
{
    LARGE_INTEGER distance{};
    distance.QuadPart = offset;

    LARGE_INTEGER new_pos{};
    DWORD move = seek_origin_to_move_method(origin);

    if (BEE_FAIL_F(FALSE != ::SetFilePointerEx(static_cast<HANDLE>(file.handle), distance, &new_pos, move), "Failed to seek file: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    return new_pos.QuadPart;
}


i64 read(const File& file, const i64 size, void* buffer)
{
    DWORD bytes_read = 0;
    const auto read_res = ::ReadFile(file.handle, buffer, static_cast<DWORD>(size), &bytes_read, nullptr);
    if (BEE_FAIL_F(read_res == TRUE, "Failed to read file: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    return bytes_read;
}

i64 write(const File& file, const void* buffer, const i64 buffer_size)
{
    DWORD bytes_written = 0;
    const auto err = ::WriteFile(
        static_cast<HANDLE>(file.handle),
        buffer,
        static_cast<DWORD>(buffer_size),
        &bytes_written,
        nullptr
    );

    if (BEE_FAIL_F(err == TRUE, "Error writing to file: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    return sign_cast<i64>(bytes_written);
}

bool is_dir(const PathView& path)
{
    const auto u16s = str::to_wchar<1024>(path.string_view());
    const auto file_attributes = ::GetFileAttributesW(u16s.data);
    return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool is_file(const PathView& path)
{
    return !is_dir(path);
}

u64 last_modified(const PathView& path)
{

    WIN32_FILE_ATTRIBUTE_DATA data{};
    const auto u16s = str::to_wchar<1024>(path.string_view());
    const auto success = ::GetFileAttributesExW(u16s.data, GetFileExInfoStandard, &data);
    if (success == 0)
    {
        log_error("Failed to get last modified time: %s", win32_get_last_error_string());
        return 0;
    }

    const auto& filetime = data.ftLastWriteTime;
    return (static_cast<u64>(filetime.dwHighDateTime) << 32) + static_cast<u64>(filetime.dwLowDateTime);
}

bool mkdir(const PathView& directory_path, const bool recursive)
{
    if (recursive)
    {
        const auto parent = directory_path.parent();
        if (!parent.exists())
        {
            mkdir(parent, recursive);
        }
    }

    const auto u16s = str::to_wchar<1024>(directory_path.string_view());
    const auto result = ::CreateDirectoryW(u16s.data, nullptr);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to make directory at path: %" BEE_PRIsv ": %s", BEE_FMT_SV(directory_path), win32_get_last_error_string());
    return false;
}

bool native_rmdir_non_recursive(const PathView& directory_path)
{
    const auto u16s = str::to_wchar<1024>(directory_path.string_view());
    const auto result = ::RemoveDirectoryW(u16s.data);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to remove directory at path: %" BEE_PRIsv ": %s", BEE_FMT_SV(directory_path), win32_get_last_error_string());
    return false;
}

bool remove(const PathView& filepath)
{
    const auto u16s = str::to_wchar<1024>(filepath.string_view());
    const auto result = ::DeleteFileW(u16s.data);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to remove file at path: %" BEE_PRIsv ": %s", BEE_FMT_SV(filepath), win32_get_last_error_string());
    return false;
}

bool move(const PathView& current_path, const PathView& new_path)
{
    const auto current_u16s = str::to_wchar<1024>(current_path.string_view());
    const auto new_u16s = str::to_wchar<1024>(new_path.string_view());
    const auto result = ::MoveFileW(current_u16s.data, new_u16s.data);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to move file from %" BEE_PRIsv " to %" BEE_PRIsv ": %s", BEE_FMT_SV(current_path), BEE_FMT_SV(new_path), win32_get_last_error_string());
    return false;
}

bool copy(const PathView& src_filepath, const PathView& dst_filepath, bool overwrite)
{
    const auto src_u16s = str::to_wchar<1024>(src_filepath.string_view());
    const auto dst_u16s = str::to_wchar<1024>(dst_filepath.string_view());
    const auto result = ::CopyFileW(src_u16s.data, dst_u16s.data, !overwrite);

    if (result != FALSE)
    {
        return true;
    }

    log_error("Unable to copy file from %" BEE_PRIsv " to %" BEE_PRIsv ": %s", BEE_FMT_SV(src_filepath), BEE_FMT_SV(dst_filepath), win32_get_last_error_string());
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
    const auto result = ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_str);
    if (BEE_FAIL_F(result == S_OK, "Couldn't retrieve local app data folder"))
    {
        return Path();
    }

    Path appdata(str::from_wchar(path_str).view());
    ::CoTaskMemFree(path_str);

    return BEE_MOVE(appdata);
}

/*
 ******************************************
 *
 * Memory mapped files - implementation
 *
 ******************************************
 */
bool mmap_file_map(MemoryMappedFile* file, const PathView& path, const OpenMode open_mode)
{
    const DWORD desired_access = decode_flag(open_mode, OpenMode::read, GENERIC_READ)
                               | decode_flag(open_mode, OpenMode::write, GENERIC_WRITE);

    const DWORD create_disposition = decode_flag(open_mode, OpenMode::read, OPEN_EXISTING)
                                   | decode_flag(open_mode, OpenMode::write, CREATE_NEW);

    const auto u16s = str::to_wchar<1024>(path.string_view());
    file->handles[0] = ::CreateFileW(
        u16s.data,
        desired_access,
        FILE_SHARE_READ,
        nullptr,
        create_disposition,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file->handles[0] == INVALID_HANDLE_VALUE)
    {
        log_error("Failed to create memory mapped file %" BEE_PRIsv ": %s", BEE_FMT_SV(path), win32_get_last_error_string());
        file->handles[0] = nullptr;
        return false;
    }

    const DWORD protect = decode_flag(open_mode, OpenMode::read, PAGE_READONLY)
                        | decode_flag(open_mode, OpenMode::write, PAGE_READWRITE);

    file->handles[1] = ::CreateFileMappingW(
        file->handles[0],
        nullptr,
        protect,
        0, 0,       // 0,0 - maxsize == sizeof file
        nullptr
    );

    if (file->handles[1] == nullptr)
    {
        log_error("Failed to memory map file %" BEE_PRIsv ": %s", BEE_FMT_SV(path), win32_get_last_error_string());
        ::CloseHandle(file->handles[0]);
        return false;
    }

    const DWORD view_access = decode_flag(open_mode, OpenMode::read, FILE_MAP_READ)
                            | decode_flag(open_mode, OpenMode::write, FILE_MAP_WRITE);
    file->data = ::MapViewOfFile(file->handles[1], view_access, 0, 0, 0);
    if (file->data == nullptr)
    {
        log_error("Failed to map file view %" BEE_PRIsv ": %s", BEE_FMT_SV(path), win32_get_last_error_string());
        ::CloseHandle(file->handles[0]);
        ::CloseHandle(file->handles[1]);
        return false;
    }

    return true;
}

bool mmap_file_unmap(MemoryMappedFile* file)
{
    if (::UnmapViewOfFile(file->data) == FALSE)
    {
        log_error("Failed to unmap file view: %s", win32_get_last_error_string());
        return false;
    }

    ::CloseHandle(file->handles[0]);
    ::CloseHandle(file->handles[1]);
    new (file) MemoryMappedFile{};
    return true;
}


} // namespace fs
} // namespace bee
