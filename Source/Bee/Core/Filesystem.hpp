/*
 *  Filesystem.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {
namespace fs {


enum class FileAction
{
    none,
    renamed,
    added,
    removed,
    modified
};


BEE_VERSIONED_HANDLE_32(DirectoryEntryHandle);

class BEE_CORE_API DirectoryIterator
{
public:
    using value_type        = Path;
    using reference         = const Path&;
    using pointer           = const Path*;

    DirectoryIterator() = default;

    explicit DirectoryIterator(const Path& directory_path);

    DirectoryIterator(const DirectoryIterator& other);

    ~DirectoryIterator();

    DirectoryIterator& operator=(const DirectoryIterator& other);

    reference operator*() const;

    pointer operator->() const;

    DirectoryIterator& operator++();

    bool operator==(const DirectoryIterator& other) const;

    inline bool operator!=(const DirectoryIterator& other) const
    {
        return !(*this == other);
    }
private:
    Path                    dir_;
    DirectoryEntryHandle    current_handle_;

    void copy_construct(const DirectoryIterator& other);

    void init();

    void next();

    void destroy();
};


struct FileNotifyInfo
{
    FileAction  action { FileAction::none };
    Path        file;
};

class BEE_CORE_API DirectoryWatcher
{
public:

    ~DirectoryWatcher();

    void start();

    void stop();

    void add_directory(const Path& path);

    inline bool is_watching() const
    {
        return watching_.load(std::memory_order_relaxed);
    }
private:
    struct Entry;

    std::atomic_bool                watching_ { false };
    DynamicArray<FileNotifyInfo>    notifications_;
    DynamicArray<Entry*>            entries_;
    void*                           native_handle_ { nullptr };
};


struct AppData
{
    Path    data_root;
    Path    logs_root;
    Path    binaries_root;
    Path    assets_root;
    Path    config_root;
};

struct Timestamp
{
    u64 year { 0 };
    u64 month { 0 };
    u64 day { 0 };
    u64 hour { 0 };
    u64 minute { 0 };
    u64 second { 0 };
    u64 millisecond { 0 };
};


BEE_CORE_API bool is_dir(const Path& path);

BEE_CORE_API bool is_file(const Path& path);

BEE_CORE_API bool last_modified(const Path& path, Timestamp* timestamp);

BEE_CORE_API String read(const Path& filepath, Allocator* allocator = system_allocator());

BEE_CORE_API FixedArray<u8> read_bytes(const Path& filepath, Allocator* allocator = system_allocator());

BEE_CORE_API bool write(const Path& filepath, const StringView& string_to_write);

BEE_CORE_API bool write(const Path& filepath, const Span<const u8>& bytes_to_write);

BEE_CORE_API bool write_v(const Path& filepath, const char* fmt_string, va_list fmt_args);

BEE_CORE_API bool remove(const Path& filepath);

BEE_CORE_API bool copy(const Path& src_filepath, const Path& dst_filepath, bool overwrite = false);

BEE_CORE_API bool mkdir(const Path& directory_path, const bool recursive = false);

BEE_CORE_API bool rmdir(const Path& directory_path, bool recursive = false);

BEE_CORE_API DirectoryIterator read_dir(const Path& directory);

BEE_CORE_API DirectoryIterator begin(const DirectoryIterator& iterator);

BEE_CORE_API DirectoryIterator end(const DirectoryIterator&);

/*
 *********************************
 *
 * Local data utilities
 *
 *********************************
 */
BEE_CORE_API const AppData& get_appdata();

BEE_CORE_API Path user_local_appdata_path();


} // namespace fs
}  // namespace bee

