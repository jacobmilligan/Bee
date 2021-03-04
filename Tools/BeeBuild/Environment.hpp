/*
 *  Environment.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"

namespace bee {


enum class BuildPlatform
{
    windows,
    unknown
};

enum class BuildIDE
{
    vs2017,
    vs2019,
    clion,
    unknown
};

struct BuildEnvironment
{
    struct WindowsEnvironment
    {
        i32     default_ide { -1 };
        BEE_PAD(4);
        Path    comspec_path;
        Path    vcvarsall_path[static_cast<int>(BuildIDE::unknown)];
        Path    cl_path[static_cast<int>(BuildIDE::unknown)];
    };

    BuildPlatform       platform { BuildPlatform::unknown };
    BEE_PAD(4);
    Path                project_root;
    Path                build_dir;
    Path                install_dir;
    Path                cmake_path;
    WindowsEnvironment  windows;
};

BEE_BUILD_API const char* to_string(const BuildPlatform platform);

BEE_BUILD_API const char* to_string(const BuildIDE ide);

BEE_BUILD_API const char* to_vs_version_string(const BuildIDE ide);

BEE_BUILD_API const char* get_local_unix_timestamp();

BEE_BUILD_API bool init_build_environment(BuildEnvironment* env);


} // namespace bee