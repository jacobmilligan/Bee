/*
 *  Filesystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Logger.hpp"

#include <stdio.h>


namespace bee {
namespace fs {


/*
 *****************************************
 *
 * DirectoryIterator - implementation
 *
 *****************************************
 */
DirectoryIterator::DirectoryIterator(const DirectoryIterator& other)
{
    copy_construct(other);
}

DirectoryIterator::~DirectoryIterator()
{
    destroy();
}

DirectoryIterator& DirectoryIterator::operator=(const DirectoryIterator& other)
{
    copy_construct(other);
    return *this;
}

void DirectoryIterator::copy_construct(const DirectoryIterator& other)
{
    path_ = other.path_;
    current_handle_ = other.current_handle_;
}

DirectoryIterator& DirectoryIterator::operator++()
{
    next();
    return *this;
}

DirectoryIterator::reference DirectoryIterator::operator*() const
{
    return path_;
}

DirectoryIterator::pointer DirectoryIterator::operator->() const
{
    return &path_;
}

bool DirectoryIterator::operator==(const bee::fs::DirectoryIterator& other) const
{
    return current_handle_ == other.current_handle_;
}

/*
 *****************************************
 *
 * DirectoryWatcher - implementation
 *
 *****************************************
 */
void DirectoryWatcher::start(const char* name)
{
    if (is_running())
    {
        log_warning("DirectoryWatcher is already running");
        return;
    }

    is_running_.store(true, std::memory_order_relaxed);

    ThreadCreateInfo thread_info{};
    thread_info.name = name == nullptr ? "Bee.DirectoryWatcher" : name;
    init(thread_info);
}

void DirectoryWatcher::suspend()
{
    is_suspended_.store(true, std::memory_order_relaxed);
}

void DirectoryWatcher::resume()
{
    is_suspended_.store(false, std::memory_order_relaxed);
}

void DirectoryWatcher::add_event(const FileAction action, const PathView& relative_path, const i32 entry)
{
    BEE_ASSERT(entry < watched_paths_.size());

    auto full_path = watched_paths_[entry].join(relative_path);
    const auto hash = get_hash(full_path);

    // On platforms like windows change notifications can sometimes fire multiple times
    const auto existing_index = find_index_if(events_, [&](const FileNotifyInfo& info)
    {
        return hash == info.hash;
    });

    if (existing_index < 0)
    {
        events_.emplace_back();
        events_.back().hash = hash;
        events_.back().event_time = time::now();
        events_.back().modified_time = last_modified(full_path.view());
        events_.back().action = action;
        events_.back().file = BEE_MOVE(full_path);
    }
    else
    {
        events_[existing_index].event_time = time::now();
    }
}

i32 DirectoryWatcher::find_entry(const PathView& path)
{
    return find_index_if(watched_paths_, [&](const Path& p)
    {
        return p == path;
    });
}

void DirectoryWatcher::pop_events(DynamicArray<FileNotifyInfo>* dst)
{
    if (!mutex_.try_lock())
    {
        return;
    }

    dst->clear();

    const u64 now = time::now();

    for (int i = events_.size() - 1; i >= 0; --i)
    {
        // file events aren't considered 'completed' until at least one 16ms frame has passed without any OS notify
        // events occurring
        if (now - events_[i].event_time > time::milliseconds(16))
        {
            dst->emplace_back(BEE_MOVE(events_[i]));
            events_.erase(i);
        }
    }

    mutex_.unlock();
}

/*
 *****************************************
 *
 * Filesystem functions - implementation
 *
 *****************************************
 */
bool native_rmdir_non_recursive(const PathView& directory_path);

bool rmdir(const PathView& directory_path, const bool recursive)
{
    if (!recursive)
    {
        return native_rmdir_non_recursive(directory_path);
    }

    bool rmdir_success = false;
    for (const auto& path : read_dir(directory_path))
    {
        if (is_dir(path))
        {
            rmdir_success = rmdir(path, true);
        }
        else
        {
            rmdir_success = remove(path);
        }

        if (!rmdir_success)
        {
            return false;
        }
    }

    return native_rmdir_non_recursive(directory_path);
}

DirectoryIterator read_dir(const PathView& directory)
{
    return DirectoryIterator(directory);
}

DirectoryIterator begin(const DirectoryIterator& iterator)
{
    return iterator;
}

DirectoryIterator end(const DirectoryIterator&)
{
    return DirectoryIterator();
}

/*
 ******************************************
 *
 * Local data utilities - implementations
 *
 ******************************************
 */
static BeeRootDirs g_roots;

void init_filesystem()
{
    auto& roots = g_roots;
    BEE_UNUSED(roots);

    // Determine if we're running from an installed build or a dev build
    const auto editor_exe_path = executable_path().parent();
    g_roots.binaries = editor_exe_path;

    // Assume that the install directory is the directory the editor .exe is running in
    g_roots.installation = editor_exe_path.parent();

    // the .exe for installed builds lives in <install root>/<version>/Binaries - dev builds live in Bee/Build/<Config>
    bool is_installed_build = editor_exe_path.parent().filename() == "Binaries";

    if (is_installed_build)
    {
        g_roots.data = fs::user_local_appdata_path();
        g_roots.data.append("Bee").append(BEE_VERSION);
    }
    else
    {
        g_roots.data.append(editor_exe_path.parent()).append("DevData");
        g_roots.installation = g_roots.installation.parent();
    }

    if (!g_roots.data.exists())
    {
        fs::mkdir(g_roots.data.view());
    }

    g_roots.logs = g_roots.data.join("Logs");

    if (!g_roots.logs.exists())
    {
        fs::mkdir(g_roots.logs.view());
    }

    /*
     * In a dev build, the assets root is located in <binaries>/../../Assets - i.e. at
     * C:/Bee/Build/Debug/../../Assets => C:/Bee/Assets.
     * Otherwise its at <binaries>/../Assets - i.e. at C:/Program Files (x86)/Bee/1.0.0/Binaries/../Assets =>
     * C:/Program Files (x86)/Bee/1.0.0/Assets
     */
    g_roots.assets =  g_roots.installation.join("Assets");

    // Installed builds have a /Config subdirectory whereas dev build output configs to /Build/<Build type>/../Config
    g_roots.configs = g_roots.installation.join("Config");

    g_roots.sources = g_roots.installation.join("Source").append("Bee");
}

void shutdown_filesystem()
{
    destruct(&g_roots);
}

const BeeRootDirs& roots()
{
    BEE_ASSERT_F(!g_roots.data.empty(), "Roots have not been initialized via bee::fs::init_roots()");
    return g_roots;
}


} // namespace fs
} // namespace bee
