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
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Memory/SmartPointers.hpp"


namespace bee {
namespace fs {


enum class FileAction
{
    none,
    added,
    removed,
    modified
};

BEE_FLAGS(FileAccess, u32)
{
    none    = 0u,
    read    = 1u << 0u,
    write   = 1u << 1u
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
    u32         hash { 0 };
    u64         modified_time { 0 };
    u64         event_time { 0 };
    FileAction  action { FileAction::none };
    Path        file;
};


struct WatchedDirectory;
struct NativeDirectoryWatcher;

class BEE_CORE_API DirectoryWatcher final : public Noncopyable
{
public:
    explicit DirectoryWatcher(const bool recursive = false);

    ~DirectoryWatcher();

    void start(const char* name);

    void stop();

    void suspend();

    void resume();

    bool add_directory(const Path& path);

    void remove_directory(const Path& path);

    void pop_events(DynamicArray<FileNotifyInfo>* dst);

    inline bool is_running() const
    {
        return is_running_.load(std::memory_order_relaxed);
    }

    inline Span<const Path> watched_directories() const
    {
        return watched_paths_.const_span();
    }

private:
    bool                                        recursive_ { false };
    std::atomic_bool                            is_running_ { false };
    std::atomic_bool                            is_suspended_ { false };
    DynamicArray<FileNotifyInfo>                events_;
    DynamicArray<UniquePtr<WatchedDirectory>>   entries_;
    DynamicArray<Path>                          watched_paths_;
    void*                                       native_ {nullptr };
    Thread                                      thread_;
    Mutex                                       mutex_;
    ConditionVariable                           start_thread_cv_;

    static void watch_loop(DirectoryWatcher* watcher);

    void init(const ThreadCreateInfo& thread_info);

    void add_event(const FileAction action, const StringView& relative_path, const i32 entry);

    i32 find_entry(const Path& path);

    void finalize_removal(const i32 index);
};

struct BeeRootDirs
{
    Path    data;
    Path    logs;
    Path    binaries;
    Path    assets;
    Path    configs;
    Path    sources;
    Path    installation;
};

BEE_CORE_API void init_filesystem();

BEE_CORE_API void shutdown_filesystem();

BEE_CORE_API bool is_dir(const Path& path);

BEE_CORE_API bool is_file(const Path& path);

BEE_CORE_API u64 last_modified(const Path& path);

BEE_CORE_API String read(const Path& filepath, Allocator* allocator = system_allocator());

BEE_CORE_API FixedArray<u8> read_bytes(const Path& filepath, Allocator* allocator = system_allocator());

BEE_CORE_API bool write(const Path& filepath, const StringView& string_to_write);

BEE_CORE_API bool write(const Path& filepath, const void* buffer, const size_t buffer_size);

BEE_CORE_API bool write_v(const Path& filepath, const char* fmt_string, va_list fmt_args);

BEE_CORE_API bool remove(const Path& filepath);

BEE_CORE_API bool move(const Path& current_path, const Path& new_path);

BEE_CORE_API bool copy(const Path& src_filepath, const Path& dst_filepath, bool overwrite = false);

BEE_CORE_API bool mkdir(const StringView& directory_path, const bool recursive = false);

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
BEE_CORE_API const BeeRootDirs& roots();

BEE_CORE_API Path user_local_appdata_path();


/*
 *********************************
 *
 * Memory mapped files
 *
 *********************************
 */
struct MemoryMappedFile
{
    FileAccess  access { FileAccess::none };
    void*       data { nullptr };
    void*       handles[2] { nullptr };
};

BEE_CORE_API bool mmap_file_map(MemoryMappedFile* file, const Path& path, const FileAccess access);

BEE_CORE_API bool mmap_file_unmap(MemoryMappedFile* file);


} // namespace fs


template <>
struct Hash<fs::FileNotifyInfo>
{
    inline u32 operator()(const fs::FileNotifyInfo& key) const
    {
        return key.hash;
    }
};


}  // namespace bee

