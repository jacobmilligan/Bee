/*
 *  PathTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Path.hpp>

#include <gtest/gtest.h>

TEST(PathTests, path_returns_correct_executable_path)
{
    // "../Build/Debug/Tests/"
    bee::Path exe("C:/A/Path/To/Build/DebugOrRelease");
    const auto parent = exe.parent();
    ASSERT_EQ(parent.filename(), "Build");
    ASSERT_EQ(parent.parent().filename(), "To");
    ASSERT_EQ(parent.parent().parent().filename(), "Path");
}

TEST(PathTests, appending_one_path_to_another_returns_correct_string)
{
    bee::Path path("/This/Is/A/Test/Path");
    bee::Path path2("So/Good");
    path.append(path2.view());
    ASSERT_EQ(path.to_generic_string(), "/This/Is/A/Test/Path/So/Good");
}

TEST(PathTests, appending_a_string_to_another_returns_correct_string)
{
    bee::Path path("/This/Is/A/Test/Path");
    path.append("So/Good");
    ASSERT_EQ(path.to_generic_string(), "/This/Is/A/Test/Path/So/Good");
}

TEST(PathTests, Setting_extension_for_path_without_extension_returns_correct_string)
{
    bee::Path path("/This/Is/A/Test/Path");
    path.set_extension("txt");
    ASSERT_EQ(path.to_generic_string(), "/This/Is/A/Test/Path.txt");
}

TEST(PathTests, Setting_extension_for_path_with_an_extension_returns_correct_string)
{
    bee::Path path("/This/Is/A/Test/Path.txt");
    path.set_extension("jpg");
    ASSERT_EQ(path.to_generic_string(), "/This/Is/A/Test/Path.jpg");
}

TEST(PathTests, make_real_removes_symlinks)
{
    auto exe_path = bee::executable_path();
    const auto last_slash_pos = bee::str::last_index_of(exe_path.string_view(), bee::Path::preferred_slash);
    auto test_path = bee::Path(bee::str::substring(exe_path.string_view(), 0, last_slash_pos))
        .append("..")
        .append("..")
        .normalize();

    auto expected_path = exe_path.parent().parent().parent();
    ASSERT_EQ(test_path, expected_path);
}

TEST(PathTests, exists_returns_true_for_paths_that_exist)
{
    auto exe_path = bee::executable_path();
    ASSERT_TRUE(exe_path.exists());
}

TEST(PathTests, exists_returns_false_for_paths_that_dont_exist)
{
    bee::Path path("/This/Is/A/Test/Path");
    ASSERT_TRUE(!path.exists());
}

TEST(PathTests, path_returns_filename_for_paths_with_extensions)
{
    bee::Path path("/This/Is/A/Test/Path.txt");
    ASSERT_EQ(path.filename(), "Path.txt");
}

TEST(PathTests, path_returns_filename_for_paths_without_extensions)
{
    bee::Path path("/This/Is/A/Test/Path");
    ASSERT_EQ(path.filename(), "Path");
}

TEST(PathTests, path_returns_filename_for_dots)
{
    bee::Path path("/This/Is/A/Test/Path\\.\\..");
    ASSERT_EQ(path.filename(), "..");
}

TEST(PathTests, path_returns_correct_parent_directory)
{
    bee::Path path("/This/Is/A/Test/Path");
    ASSERT_EQ(path.parent(), "/This/Is/A/Test");

    bee::Path path2("/Users/Jacob/Dev/Repos/Bee/Build/Debug/Tests/Static/Platform/PlatformTests");
    ASSERT_EQ(path2.parent(), "/Users/Jacob/Dev/Repos/Bee/Build/Debug/Tests/Static/Platform");
}

TEST(PathTests, stem_returns_just_the_filename_component_without_extension)
{
    bee::Path path("/This/Is/A/Test/Path.txt");
    ASSERT_EQ(path.stem(), "Path");
}

TEST(PathTests, relative_path_returns_correct_string)
{
    bee::Path path("/This/Is/A/Test/Path");
    ASSERT_EQ(path.relative_path(), "This/Is/A/Test/Path");
    ASSERT_EQ(path.relative_path().relative_path(), "Is/A/Test/Path");

    // Test windows drive letters
    auto win_path = bee::Path("C:\\This\\Is\\A\\Test\\Path");
    ASSERT_EQ(path.relative_path(), "This/Is/A/Test/Path");
    ASSERT_EQ(path.relative_path().relative_path(), "Is/A/Test/Path");
}

TEST(PathTests, paths_report_correct_size)
{
    auto str = "/This/Is/A/Test/Path";
    auto len = strlen(str);
    bee::Path path(str);
    ASSERT_TRUE(path.size() == len);

    auto str2 = "/This/Is/A/Test/Path/../../../with/weird/stuff.txt.hey";
    len = strlen(str2);
    path = bee::Path(str2);
    ASSERT_TRUE(path.size() == len);
}

TEST(PathTests, path_equality)
{
    bee::Path path("/This/Is/A/Test/Path");
    bee::Path path2("/This\\Is/A/Test\\Path");
    bee::Path path3("/This/Is/A/Test/Path/../..");
    bee::Path path4("/This/I1s/A/Test2/Path/../..");
    bee::Path generic_win("C:/This/I1s\\A/Test2/Path/../..");
    bee::Path native_win("C:\\This/I1s\\A\\Test2\\Path\\..\\..");

    ASSERT_EQ(path, path2);
    ASSERT_EQ(generic_win, native_win);
    ASSERT_NE(path, path3);
    ASSERT_NE(path3, path4);
    ASSERT_NE(path4, generic_win);
    ASSERT_NE(path4, native_win);
}

TEST(PathTests, path_iterator)
{
    bee::Path win_path("C:\\This\\Is\\A\\Test\\Path\\..\\..");
    const char* parts[] = { "C:", "This", "Is", "A", "Test", "Path", "..", ".." };

    int part_idx = 0;
    auto iter = win_path.begin();
    const auto end = win_path.end();
    for (; iter != end; ++iter)
    {
        ASSERT_EQ(*iter, parts[part_idx]);
        ++part_idx;
    }
}

TEST(PathTests, relative_to)
{
    /* Examples:
     * "D:\Root" ("D:\Root\Another\Path") -> "..\..\Root"
     * "D:\Root\Another\Path" ("D:\Root") -> "Another\Path"
     * "D:\Root" ("C:\Root") -> "..\..\D:\Root"
     * "/a/d" ("/b/c") -> "../../a/d"
     */
    bee::Path path("D:\\Root");
    auto relative = path.relative_to("D:\\Root\\Another\\Path");
    ASSERT_EQ(relative.view(), "..\\..");

    path = "D:\\Root\\Another\\Path";
    relative = path.relative_to("D:\\Root");
    ASSERT_EQ(relative.view(), "Another\\Path");

    path = "D:\\Root";
    relative = path.relative_to("C:\\Root");
    ASSERT_EQ(relative.view(), "..\\..\\D:\\Root");

    path = "/a/d";
    auto generic_str = path.relative_to("/b/c").to_generic_string();
    ASSERT_EQ(generic_str, "../../a/d");

    path = "D:\\Root\\test.txt";
    relative = path.relative_to("D:\\Root\\Another\\Path");
    ASSERT_EQ(relative.view(), "..\\..\\test.txt");

    path = "D:/Code/Bee/Tools/ImGuiGenerator/Generator.inl";
    auto output_dir = bee::PathView("D:/Code/Bee/Build/Generated/ReflectTest");
    auto other = bee::Path(output_dir)
        .append(path.filename())
        .set_extension("generated")
        .append_extension(".cpp");
    relative = path.relative_to(other.view().parent()).make_generic();
    ASSERT_EQ(relative.view(), "../../../Tools/ImGuiGenerator/Generator.inl");
}

TEST(PathTests, string_view_comparison)
{
    bee::StringView sv = "Bee.AssetPipeline.dll";
    bee::Path sv_path = "Bee.AssetPipeline.dll";
    bee::Path path = "Bee.AssetPipeline.pdb";
    bee::Path path_with_slashes = "Bee.Asset/Pipel/ine.pdb";
    bee::Path path_with_repeated_slashes = "Bee.Asset////////Pipel//////////ine.pdb";
    bee::Path path_with_slashes2 = "Bee.Asset/Pipel/ine.dll";
    ASSERT_NE(sv_path, path);
    ASSERT_NE(sv, path);
    ASSERT_NE(path_with_slashes, path);
    ASSERT_NE(path_with_slashes, path_with_slashes2);
    ASSERT_EQ(path_with_slashes, path_with_repeated_slashes);
}
