/*
 *  Filesystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Error.hpp"

#include <stdio.h>
#include <fstream>


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
 * Filesystem functions - implementation
 *
 *****************************************
 */
String read(const Path& filepath, Allocator* allocator)
{
    auto file = fopen(filepath.c_str(), "rb");
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

    return std::move(result);
}

FixedArray<u8> read_bytes(const Path& filepath, Allocator* allocator)
{
    auto file = fopen(filepath.c_str(), "rb");
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

    return std::move(result);
}

bool write(const Path& filepath, const String& string_to_write)
{
    return write(filepath, string_to_write.view());
}

bool write(const Path& filepath, const StringView& string_to_write)
{
    auto file = fopen(filepath.c_str(), "wb");
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
    auto file = fopen(filepath.c_str(), "wb");
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
    auto file = fopen(filepath.c_str(), "wb");
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

    bool rmdir_success;
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
const AppData& get_appdata()
{
    static AppData appdata{};

    if (!appdata.root.empty())
    {
        // Already discovered appdata previously
        return appdata;
    }

    // Determine if we're running from an installed build or a dev build
    const auto editor_exe_path = Path::executable_path().parent();
    bool is_installed_build = editor_exe_path.filename() != "Debug" && editor_exe_path.filename() != "Release";

    appdata.root = is_installed_build
                   ? fs::user_local_appdata_path().join("Bee").join(BEE_VERSION)
                   : editor_exe_path.parent().join("DevData");

    if (!appdata.root.exists())
    {
        fs::mkdir(appdata.root);
    }

    appdata.logs_root = appdata.root.join("Logs");

    if (!appdata.logs_root.exists())
    {
        fs::mkdir(appdata.logs_root);
    }

    // Assume that the install directory is the directory the editor .exe is running in
    appdata.binaries_root = editor_exe_path;

    /*
     * In a dev build, the assets root is located in <binaries_root>/../../Assets - i.e. at
     * C:/Skyrocket/Build/Debug/../../Assets => C:/Skyrocket/Assets.
     * Otherwise its at <binaries_root>/../Assets - i.e. at C:/Program Files (x86)/Skyrocket/1.0.0/Binaries/../Assets =>
     * C:/Program Files (x86)/Skyrocket/1.0.0/Assets
     */
    appdata.assets_root = is_installed_build
                        ? appdata.binaries_root.parent().join("Assets")
                        : appdata.binaries_root.parent().parent().join("Assets");

    // Installed builds have a /Config subdirectory whereas dev build output configs to /Build/<Build type>/../Config
    appdata.config_root = is_installed_build
                          ? appdata.binaries_root.join("Config")
                          : appdata.binaries_root.parent().join("Config");

    return appdata;
}


} // namespace fs
} // namespace bee
