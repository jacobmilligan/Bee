/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Process.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Logger.hpp"

namespace bee {


struct BuildInfo
{
    Path project_root;
    Path build_dir;
    Path install_dir;
    Path cmake_path;
};

const BuildInfo& build_info()
{
    static BuildInfo info;

    if (info.project_root.empty())
    {
        info.project_root = Path::executable_path().parent().parent().parent();
        info.build_dir = info.project_root.join("Build");
#if BEE_OS_WINDOWS == 1
        info.cmake_path = info.project_root.join("ThirdParty/Binaries/Win32/cmake/bin/cmake.exe").normalize();
#else
        #error Platform not supported
#endif // BEE_OS_WINDOWS == 1
    }

    return info;
}


int configure(const char* bb_generator, const char* cmake_generator, const char* build_type = nullptr)
{
    auto& info = build_info();
    auto build_dir = info.build_dir.join(bb_generator);
    if (build_type != nullptr)
    {
        build_dir.join(build_type);
    }

    const auto install_dir = build_dir.join("Install");

    String cmd;
    io::StringStream stream(&cmd);
    stream.write_fmt(
        "%s %s -G %s -B %s -DCMAKE_INSTALL_PREFIX=\"%s\"",
        info.cmake_path.c_str(),
        info.project_root.c_str(),
        cmake_generator,
        build_dir.c_str(),
        install_dir.c_str()
    );

    if (build_type != nullptr)
    {
        stream.write_fmt(" -DCMAKE_BUILD_TYPE=%s", build_type);
    }

    log_info("Running CMake with command: %s", cmd.c_str());

    bee::ProcessHandle cmake_handle{};
    bee::CreateProcessInfo proc_info{};
    proc_info.handle = &cmake_handle;
    proc_info.flags = bee::CreateProcessFlags::priority_high | bee::CreateProcessFlags::create_hidden;
    proc_info.command_line = cmd.c_str();

    if (!bee::create_process(proc_info))
    {
        bee::log_error("Launchpad: Unable to find cmake");
        return EXIT_FAILURE;
    }

    bee::wait_for_process(cmake_handle);
    bee::destroy_process(cmake_handle);

    return EXIT_SUCCESS;
}

int build(const String& cmake_cmd)
{
    return EXIT_SUCCESS;
}

int bb_entry(int argc, char** argv)
{
    FixedHashMap<String, String> generator_mappings =
    {
        { "VS2017", "\"Visual Studio 15 2017 Win64\"" },
        { "CLion", "\"CodeBlocks - NMake Makefiles\"" }
    };

    String generator_help = "Generator to use when configuring build system.\nAvailable generators (bb => cmake):\n";
    for (const auto& mapping : generator_mappings)
    {
        generator_help += "  - " + mapping.key + " => " + mapping.value + "\n";
    }

    cli::Option options[] =
    {
        cli::Option('c', "configure", false, "Configures and generates project files instead of building", 0),
        cli::Option('g', "generator", true, generator_help.c_str(), 1)
    };

    const auto command_line = cli::parse(argc, argv, nullptr, 0, options, static_array_length(options));

    if (!command_line.success)
    {
        log_error("%s", command_line.error_message.c_str());
        return EXIT_FAILURE;
    }

    if (command_line.help_requested)
    {
        log_info("%s", command_line.help_string.c_str());
        return EXIT_SUCCESS;
    }

    const auto generator = cli::get_option(command_line, "generator");
    // Project root is always at: <Root>/<BuildConfig>/bb.exe/../../../
    const auto cmake_generator = generator_mappings.find(generator);
    if (cmake_generator == nullptr)
    {
        log_error("Invalid generator specified: %s", generator);
        return EXIT_FAILURE;
    }

//    cmd.write_fmt("cmake ");

    if (cli::has_option(command_line, "configure"))
    {
        if (str::compare(generator, "CLion") == 0)
        {
            return configure(generator, cmake_generator->value.c_str(), "Debug")
                && configure(generator, cmake_generator->value.c_str(), "Release");
        }

        return configure(generator, cmake_generator->value.c_str());
    }

    return build(String());
}


} // namespace bee


int main(int argc, char** argv)
{
    return bee::bb_entry(argc, argv);
}
