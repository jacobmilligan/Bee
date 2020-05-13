/*
 *  FilesystemTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Filesystem.hpp>

#include <gtest/gtest.h>

TEST(FilesystemTests, appdata)
{
    const auto& engine_appdata = bee::fs::get_appdata();
    ASSERT_STREQ(engine_appdata.data_root.filename().c_str(), "DevData");
    ASSERT_TRUE(engine_appdata.binaries_root.filename() == "Debug" || engine_appdata.binaries_root.filename() == "Release");
    ASSERT_STREQ(engine_appdata.logs_root.filename().c_str(), "Logs");
    ASSERT_STREQ(engine_appdata.config_root.filename().c_str(), "Config");

    const auto local_appdata = bee::fs::user_local_appdata_path();
#if BEE_OS_WINDOWS == 1
    ASSERT_STREQ(local_appdata.filename().c_str(), "Local");
    ASSERT_STREQ(local_appdata.parent_path().filename().c_str(), "AppData");
#endif // BEE_OS_WINDOWS == 1
}

TEST(FilesystemTests, read_write_file)
{
    static constexpr auto test_string = "This is a test string";
    static constexpr bee::u8 test_bytes[] = { 1, 2, 3, 4, 5, 6 };

    const auto filepath = bee::fs::get_appdata().data_root.join("TestFile.txt");

    ASSERT_FALSE(filepath.exists());
    ASSERT_TRUE(bee::fs::write(filepath, test_string));
    ASSERT_TRUE(filepath.exists());

    const auto read_string = bee::fs::read(filepath);
    ASSERT_STREQ(read_string.c_str(), test_string);

    ASSERT_TRUE(bee::fs::remove(filepath));

    // Test writing bytes
    ASSERT_TRUE(bee::fs::write(filepath, bee::Span<const bee::u8>(test_bytes)));
    ASSERT_TRUE(filepath.exists());

    const auto read_bytes = bee::fs::read_bytes(filepath);
    ASSERT_EQ(read_bytes.size(), bee::static_array_length(test_bytes));
    for (int b = 0; b < read_bytes.size(); ++b)
    {
        ASSERT_EQ(read_bytes[b], test_bytes[b]);
    }

    ASSERT_TRUE(bee::fs::remove(filepath));
}

TEST(FilesystemTests, copy_file)
{
    static constexpr auto test_string = "This is a test string";
    const auto src_filepath = bee::fs::get_appdata().data_root.join("TestFile.txt");
    const auto dst_filepath = bee::fs::get_appdata().data_root.join("TestFile2.txt");

    ASSERT_FALSE(src_filepath.exists());
    ASSERT_TRUE(bee::fs::write(src_filepath, test_string));
    ASSERT_TRUE(src_filepath.exists());
    ASSERT_TRUE(bee::fs::copy(src_filepath, dst_filepath));

    const auto dst_string = bee::fs::read(dst_filepath);

    ASSERT_STREQ(dst_string.c_str(), test_string);
    ASSERT_TRUE(bee::fs::remove(src_filepath));
    ASSERT_TRUE(bee::fs::remove(dst_filepath));
}

TEST(FilesystemTests, make_and_remove_directory)
{
    const auto dirpath = bee::fs::get_appdata().data_root.join("NonRecursiveTestDir");
    if (!dirpath.exists())
    {
        ASSERT_TRUE(bee::fs::mkdir(dirpath));
    }
    ASSERT_TRUE(dirpath.exists());
    ASSERT_TRUE(bee::fs::rmdir(dirpath));
}

TEST(FilesystemTests, make_and_remove_directory_recursive)
{
    const auto dirpath = bee::fs::get_appdata().data_root.join("RecursiveTestDir");
    const bee::Path test_paths[] = {
        dirpath,
        dirpath.join("Nested"),
        dirpath.join("Nested").join("Nested2"),
        dirpath.join("Nested").join("Text.txt"),
        dirpath.join("Nested").join("Nested2").join("Text.txt")
    };

    for (const auto& path : test_paths)
    {
        if (bee::fs::is_dir(path) && !path.exists())
        {
            ASSERT_TRUE(bee::fs::mkdir(path));
            continue;
        }
        else
        {
            ASSERT_TRUE(bee::fs::write(path, "Test text"));
        }
    }

    ASSERT_TRUE(dirpath.exists());
    ASSERT_TRUE(bee::fs::rmdir(dirpath, true));

    for (const auto& path : test_paths)
    {
        ASSERT_FALSE(path.exists());
    }
}

void read_dir_recursive(const bee::Path& root, const bee::Span<bee::Path>& test_data, int* recursive_calls)
{
    ++(*recursive_calls);
    
    for (const auto& path : bee::fs::read_dir(root))
    {
        if (root == path)
        {
            continue;
        }

        // Check if the current path is valid in the test data
        int index = 0;
        for (const auto& t : test_data)
        {
            if (t == path)
            {
                break;
            }
            ++index;
        }

        ASSERT_LT(index, test_data.size());
        test_data[index].clear();

        if (bee::fs::is_dir(path))
        {
            read_dir_recursive(path, test_data, recursive_calls);
        }
    }
}

TEST(FilesystemTests, read_directory)
{
    static constexpr int max_nested_dir_level = 4;
    static constexpr auto test_string = "This is a test string";
    const auto dirpath = bee::fs::get_appdata().data_root.join("TestDir\\");

    // Make a bunch of test folders and files
    bee::Path test_data[] = {
        dirpath.join("TestFile.txt"),
        dirpath.join("TestFile2.md"),
        dirpath.join("TestDir1"),
        dirpath.join("TestDir1/TestFile.txt"),
        dirpath.join("TestDir1/Nested"),
        dirpath.join("TestDir1/Nested/TestFile.txt"),
        dirpath.join("TestDir2"),
        dirpath.join("TestDir2/TestFile2.txt")
    };

    if (!dirpath.exists())
    {
        bee::fs::mkdir(dirpath);
    }
    ASSERT_TRUE(dirpath.exists());

    for (const auto& path : test_data)
    {
        if (path.exists())
        {
            continue;
        }

        if (path.extension().empty())
        {
            bee::fs::mkdir(path);
        }
        else
        {
            bee::fs::write(path, test_string);
        }
    }

    int recursive_calls = 0;
    read_dir_recursive(dirpath, bee::Span<bee::Path>(test_data), &recursive_calls);

    int empty_count = 0;
    for (const auto& path : test_data)
    {
        if (path.empty())
        {
            ++empty_count;
        }
    }
    ASSERT_EQ(empty_count, bee::static_array_length(test_data));
    ASSERT_EQ(recursive_calls, max_nested_dir_level);

    ASSERT_TRUE(bee::fs::rmdir(dirpath, true));
}
