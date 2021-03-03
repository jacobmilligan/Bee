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
#include "Bee/Core/IO.hpp"
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

BEE_FLAGS(OpenMode, u32)
{
    none    = 0u,
    read    = 1u << 0u,
    write   = 1u << 1u,
    append  = 1u << 2u
};


BEE_VERSIONED_HANDLE_32(DirectoryEntryHandle);

class BEE_CORE_API DirectoryIterator
{
public:
    using value_type        = PathView;
    using reference         = const PathView&;
    using pointer           = const PathView*;

    DirectoryIterator() = default;

    explicit DirectoryIterator(const PathView& directory_path);

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
    PathView                path_;
    DirectoryEntryHandle    current_handle_;
    BEE_PAD(4);

    void copy_construct(const DirectoryIterator& other);

    void next();

    void destroy();
};


struct FileNotifyInfo
{
    u64         modified_time { 0 };
    u64         event_time { 0 };
    u32         hash { 0 };
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

    bool add_directory(const PathView& path);

    void remove_directory(const PathView& path);

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
    DynamicArray<FileNotifyInfo>                events_;
    DynamicArray<UniquePtr<WatchedDirectory>>   entries_;
    DynamicArray<Path>                          watched_paths_;
    void*                                       native_ {nullptr };
    Thread                                      thread_;
    Mutex                                       mutex_;
    ConditionVariable                           start_thread_cv_;
    bool                                        recursive_ { false };
    std::atomic_bool                            is_running_ { false };
    std::atomic_bool                            is_suspended_ { false };
    BEE_PAD(5);

    static void watch_loop(DirectoryWatcher* watcher);

    void init(const ThreadCreateInfo& thread_info);

    void add_event(const FileAction action, const PathView& relative_path, const i32 entry);

    i32 find_entry(const PathView& path);

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

struct BEE_CORE_API File final : public Noncopyable
{
    void*       handle { nullptr };
    OpenMode    mode { OpenMode::none };
    BEE_PAD(4);

    File() = default;

    File(void* handle, const OpenMode open_mode)
        : handle(handle),
          mode(open_mode)
    {}

    File(File&& other) noexcept;

    File& operator=(File&& other) noexcept;

    ~File();

    inline bool is_valid() const
    {
        return handle != nullptr && mode != OpenMode::none;
    }

    inline operator bool() const
    {
        return is_valid();
    }
};

BEE_CORE_API void init_filesystem();

BEE_CORE_API void shutdown_filesystem();

BEE_CORE_API bool is_dir(const PathView& path);

BEE_CORE_API bool is_file(const PathView& path);

BEE_CORE_API u64 last_modified(const PathView& path);

BEE_CORE_API File open_file(const PathView& path, const OpenMode mode);

BEE_CORE_API void close_file(File* file);

BEE_CORE_API i64 get_size(const File& file);

BEE_CORE_API i64 tell(const File& file);

BEE_CORE_API i64 seek(const File& file, const i64 offset, const io::SeekOrigin origin);

BEE_CORE_API i64 read(const File& file, const i64 size, void* buffer);

BEE_CORE_API String read_all_text(const File& file, Allocator* allocator = system_allocator());

BEE_CORE_API FixedArray<u8> read_all_bytes(const File& file, Allocator* allocator = system_allocator());

BEE_CORE_API String read_all_text(const PathView& path, Allocator* allocator = system_allocator());

BEE_CORE_API FixedArray<u8> read_all_bytes(const PathView& path, Allocator* allocator = system_allocator());

BEE_CORE_API i64 write(const File& file, const StringView& string_to_write);

BEE_CORE_API i64 write(const File& file, const void* buffer, const i64 buffer_size);

BEE_CORE_API i64 write_all(const PathView& path, const StringView& string_to_write);

BEE_CORE_API i64 write_all(const PathView& path, const void* buffer, const i64 buffer_size);

BEE_CORE_API i64 append_all(const PathView& path, const StringView& string_to_write);

BEE_CORE_API i64 append_all(const PathView& path, const void* buffer, const i64 buffer_size);

BEE_CORE_API bool remove(const PathView& filepath);

BEE_CORE_API bool move(const PathView& current_path, const PathView& new_path);

BEE_CORE_API bool copy(const PathView& src_filepath, const PathView& dst_filepath, bool overwrite = false);

BEE_CORE_API bool mkdir(const PathView& directory_path, const bool recursive = false);

BEE_CORE_API bool rmdir(const PathView& path, const bool recursive = false);

BEE_CORE_API DirectoryIterator read_dir(const PathView& directory);

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
    void*       data { nullptr };
    void*       handles[2] { nullptr };
    OpenMode    mode { OpenMode::none };
    BEE_PAD(4);
};

BEE_CORE_API bool mmap_file_map(MemoryMappedFile* file, const PathView& path, const OpenMode open_mode);

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

