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
DirectoryIterator::DirectoryIterator(const Path& directory_path)
    : dir_(directory_path)
{
    init();
}

DirectoryIterator::DirectoryIterator(const DirectoryIterator& other)
{
    copy_construct(other);
}

DirectoryIterator& DirectoryIterator::operator=(const DirectoryIterator& other)
{
    copy_construct(other);
    return *this;
}

void DirectoryIterator::copy_construct(const DirectoryIterator& other)
{
    dir_ = other.dir_;
    current_handle_ = other.current_handle_;
}

DirectoryIterator& DirectoryIterator::operator++()
{
    next();
    return *this;
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

void DirectoryWatcher::add_event(const FileAction action, const StringView& relative_path, const i32 entry)
{
    BEE_ASSERT(entry < watched_paths_.size());

    // Need to re-init the array if it was moved previously via `pop_events`
    if (events_.allocator() == nullptr)
    {
        new (&events_) DynamicArray<FileNotifyInfo>{};
    }

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
        events_.back().action = action;
        events_.back().file = std::move(full_path);
    }
}

i32 DirectoryWatcher::find_entry(const Path &path)
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
    dst->append(events_.const_span());
    events_.clear();

    mutex_.unlock();
}

/*
 *****************************************
 *
 * Filesystem functions - implementation
 *
 *****************************************
 */
String read(const Path& filepath, Allocator* allocator)
{
    auto* file = fopen(filepath.c_str(), "rb");
    if (BEE_FAIL_F(file != nullptr, "No such file found with the specified name %s", filepath.c_str()))
    {
        return "";
    }

    fseek(file, 0, SEEK_END);
    const auto file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    String result(file_length, ' ', allocator);
    const auto chars_read = fread(result.data(), 1, file_length, file);
    BEE_ASSERT_F(static_cast<long>(chars_read) == file_length, "Failed to read entire file");

    fclose(file);

    return result;
}

FixedArray<u8> read_bytes(const Path& filepath, Allocator* allocator)
{
    auto* file = fopen(filepath.c_str(), "rb");
    if (BEE_FAIL_F(file != nullptr, "No such file found with the specified name %s", filepath.c_str()))
    {
        return {};
    }

    fseek(file, 0, SEEK_END);
    const auto file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    FixedArray<u8> result = FixedArray<u8>::with_size(file_length, allocator);

    const auto chars_read = fread(result.data(), 1, file_length, file);
    BEE_ASSERT_F(static_cast<long>(chars_read) == file_length, "Failed to read entire file");
    fclose(file);

    return result;
}

bool write(const Path& filepath, const String& string_to_write)
{
    return write(filepath, string_to_write.view());
}

bool write(const Path& filepath, const StringView& string_to_write)
{
    auto* file = fopen(filepath.c_str(), "wb");
    if (BEE_FAIL_F(file != nullptr, "Unable to open or write to file: %s", filepath.c_str()))
    {
        return false;
    }

    fwrite(string_to_write.c_str(), 1, string_to_write.size(), file);
    fclose(file);
    return true;
}

bool write(const Path& filepath, const Span<const u8>& bytes_to_write)
{
    auto* file = fopen(filepath.c_str(), "wb");
    if (BEE_FAIL_F(file != nullptr, "Unable to open or write to file: %s", filepath.c_str()))
    {
        return false;
    }

    fwrite(bytes_to_write.data(), sizeof(const u8), bytes_to_write.size(), file);
    fclose(file);
    return true;
}

bool write_v(const Path& filepath, const char* fmt_string, va_list fmt_args)
{
    auto* file = fopen(filepath.c_str(), "wb");
    if (BEE_FAIL_F(file != nullptr, "Unable to open or write to file: %s", filepath.c_str()))
    {
        return false;
    }

    vfprintf(file, fmt_string, fmt_args);
    fclose(file);
    return true;
}

bool native_rmdir_non_recursive(const Path& directory_path);

bool rmdir(const Path& directory_path, const bool recursive)
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

DirectoryIterator read_dir(const Path& directory)
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
const BeeRootDirs& get_root_dirs()
{
    static BeeRootDirs appdata{};

    if (!appdata.data_root.empty())
    {
        // Already discovered appdata previously
        return appdata;
    }

    // Determine if we're running from an installed build or a dev build
    const auto editor_exe_path = Path(Path::executable_path().parent_path());

    // the .exe for installed builds lives in <install root>/<version>/Binaries - dev builds live in Bee/Build/<Config>
    bool is_installed_build = editor_exe_path.parent_path().filename() == "Binaries";

    appdata.data_root = is_installed_build
                      ? fs::user_local_appdata_path().join("Bee").join(BEE_VERSION)
                      : editor_exe_path.parent_path().join("DevData");

    if (!appdata.data_root.exists())
    {
        fs::mkdir(appdata.data_root);
    }

    appdata.logs_root = appdata.data_root.join("Logs");

    if (!appdata.logs_root.exists())
    {
        fs::mkdir(appdata.logs_root);
    }

    // Assume that the install directory is the directory the editor .exe is running in
    appdata.binaries_root = editor_exe_path;
    appdata.install_root = is_installed_build
                         ? appdata.binaries_root.parent_path()
                         : appdata.binaries_root.parent_path().parent_path();

    /*
     * In a dev build, the assets root is located in <binaries_root>/../../Assets - i.e. at
     * C:/Bee/Build/Debug/../../Assets => C:/Bee/Assets.
     * Otherwise its at <binaries_root>/../Assets - i.e. at C:/Program Files (x86)/Bee/1.0.0/Binaries/../Assets =>
     * C:/Program Files (x86)/Bee/1.0.0/Assets
     */
    appdata.assets_root =  appdata.install_root.join("Assets");

    // Installed builds have a /Config subdirectory whereas dev build output configs to /Build/<Build type>/../Config
    appdata.config_root = appdata.install_root.join("Config");

    appdata.sources_root = appdata.install_root.join("Source");

    return appdata;
}


} // namespace fs
} // namespace bee
