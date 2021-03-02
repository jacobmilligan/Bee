/*
 *  Environment.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */


#include "BeeBuild/Environment.hpp"

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/Filesystem.hpp"

#include <time.h>


namespace bee {


BEE_TRANSLATION_TABLE_FUNC(to_string, BuildPlatform, const char*, BuildPlatform::unknown,
    "Windows"                 // windows
)

BEE_TRANSLATION_TABLE_FUNC(to_string, BuildIDE, const char*, BuildIDE::unknown,
    "VS2017",   // vs2017
    "VS2019",   // vs2019
    "CLion"     // clion
)

BEE_TRANSLATION_TABLE_FUNC(to_vs_version_string, BuildIDE, const char*, BuildIDE::unknown,
    "15.0",     // vs2017
    "16.0",     // vs2019
    nullptr     // clion
)

const char* get_local_unix_timestamp()
{
    static thread_local StaticString<256> timestamp(256, '\0');
    const time_t timepoint = ::time(nullptr);
    auto* const timeinfo = localtime(&timepoint);
    ::strftime(timestamp.data(), timestamp.capacity(), BEE_TIMESTAMP_FMT, timeinfo);
    return timestamp.c_str();
}

bool init_build_environment(BuildEnvironment* env)
{
#if BEE_OS_WINDOWS == 1

    env->platform = BuildPlatform::windows;
#else
    env->platform = BuildPlatform::unknown;
#endif // BEE_OS_WINDOWS == 1

    if (env->platform == BuildPlatform::unknown)
    {
        return false;
    }

    env->project_root = executable_path().parent().parent().parent();
    env->build_dir = env->project_root.join("Build");

    const auto bin_root = env->project_root.join("ThirdParty/Binaries");

    if (env->platform == BuildPlatform::windows)
    {
        env->cmake_path = bin_root.join("cmake/bin/cmake.exe").normalize();

        // Get the path to vcvarsall - this is a complicated process so buckle up...

        // Run cmake in a shell with vcvarsall if the CLion generator is used otherwise NMake won't know where to find VS
        const auto vswhere_location = bin_root.join("vswhere.exe");
        DynamicArray<StringView> vs_versions(temp_allocator());
        Path version_path(temp_allocator());

        // Run vswhere to get VS2017 install directory
        for (int i = 0; i < static_cast<int>(BuildIDE::unknown); ++i)
        {
            const char* vs_version = to_vs_version_string(static_cast<BuildIDE>(i));
            if (vs_version == nullptr || str::length(vs_version) <= 0)
            {
                continue;
            }

            auto vswhere_cmd = str::format(
                "%s -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -version %s",
                vswhere_location.c_str(), vs_version
            );

            bee::ProcessHandle vswhere{};

            bee::CreateProcessInfo proc_info{};
            proc_info.handle = &vswhere;
            proc_info.flags = bee::CreateProcessFlags::priority_high
                | bee::CreateProcessFlags::create_hidden
                | bee::CreateProcessFlags::create_read_write_pipes;
            proc_info.command_line = vswhere_cmd.c_str();

            if (!bee::create_process(proc_info))
            {
                bee::log_error("Couldn't find vswhere.exe - unable to use CLion generator");
                return false;
            }

            bee::wait_for_process(vswhere);
            auto vswhere_out = bee::read_process(vswhere);
            bee::destroy_process(vswhere);
            bee::str::split(vswhere_out.view(), &vs_versions, "\r\n");

            const auto vs_location = vs_versions.back();

            if (vs_versions.empty())
            {
                continue;
            }

            if (env->windows.default_ide < 0)
            {
                env->windows.default_ide = i;
            }

            // Setup path to vcvarsall.bat so we can run the shell with all the VS vars
            env->windows.vcvarsall_path[i].clear();
            env->windows.vcvarsall_path[i].append(vs_location).append("VC\\Auxiliary\\Build\\vcvarsall.bat");
            env->windows.vcvarsall_path[i].normalize();

            // Get path to cl.exe (see: https://github.com/microsoft/vswhere/wiki/Find-VC)
            version_path.clear();
            version_path.append(vs_location).append("VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt");
            if (!version_path.exists())
            {
                log_error("Failed to find visual studio version file");
                return false;
            }

            auto version = fs::read_all_text(version_path.view(), temp_allocator());
            str::trim(&version, '\r');
            str::trim(&version, '\n');

            if (version.empty())
            {
                log_error("Failed to get visual studio install version location");
                return false;
            }

            env->windows.cl_path[i].clear();
            env->windows.cl_path[i].append(vs_location)
                .append("VC\\Tools\\MSVC")
                .append(version.view())
                .append("bin\\Hostx64\\x64\\cl.exe");

            if (!env->windows.cl_path[i].exists())
            {
                log_error("Missing cl.exe at: %s", env->windows.cl_path[i].c_str());
                return false;
            }
        }
    }

    if (env->windows.default_ide < 0)
    {
        bee::log_error("Couldn't find a visual studio installation on this machine");
        return false;
    }

    String cmd_exe(temp_allocator());
    if (!get_environment_variable("COMSPEC", &cmd_exe))
    {
        log_error("Failed to find cmd.exe");
        return EXIT_FAILURE;
    }

    env->windows.comspec_path.clear();
    env->windows.comspec_path.append(cmd_exe.view());

    return true;
}




} // namespace bee