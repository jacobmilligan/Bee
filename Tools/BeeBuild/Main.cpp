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

#if BEE_OS_WINDOWS == 1
    Path vcvarsall_path;
#endif // BEE_OS_WINDOWS == 1
};


struct ConfigureInfo
{
    const char*         bb_generator { nullptr };
    const char*         cmake_generator { nullptr };
    const char*         build_type { nullptr };
    i32                 cmake_options_count { 0 };
    const char* const*  cmake_options { nullptr };
};

const BuildInfo& get_build_info()
{
    static BuildInfo info;

    if (info.project_root.empty())
    {
        info.project_root = Path::executable_path().parent().parent().parent();
        info.build_dir = info.project_root.join("Build");
#if BEE_OS_WINDOWS == 1
        const auto win32_bin_root = info.project_root.join("ThirdParty/Binaries/Win32");

        info.cmake_path = win32_bin_root.join("cmake/bin/cmake.exe").normalize();

        // Get the path to vcvarsall - this is a complicated process so buckle up...

        // Run cmake in a shell with vcvarsall if the CLion generator is used otherwise NMake won't know where to find VS
        const auto vswhere_location = win32_bin_root.join("vswhere.exe");
        auto vswhere_cmd = str::format(
            "%s -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath",
            vswhere_location.c_str()
        );

        // Run vswhere to get VS2017 install directory
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
        }
        else
        {
            bee::wait_for_process(vswhere);
            auto vs_location = bee::read_process(vswhere);
            bee::destroy_process(vswhere);

            bee::str::replace(&vs_location, "\r\n", "");
            if (vs_location.empty())
            {
                bee::log_error("Couldn't find a visual studio installation on this machine");
            }
            else
            {
                // Setup path to vcvarsall.bat so we can run the shell with all the VS vars
                info.vcvarsall_path = str::format("%s\\VC\\Auxiliary\\Build\\vcvarsall.bat", vs_location.c_str()).view();
                info.vcvarsall_path.normalize();
            }
        }
#else
        #error Platform not supported
#endif // BEE_OS_WINDOWS == 1
    }

    return info;
}

bool configure(const ConfigureInfo& config_info)
{
    auto& build_info = get_build_info();
    auto build_dir = build_info.build_dir.join(config_info.bb_generator);
    if (config_info.build_type != nullptr)
    {
        build_dir.join(config_info.build_type);
    }

    const auto install_dir = build_dir.join("Install");

    String cmd;
    io::StringStream stream(&cmd);

    if (str::compare(config_info.bb_generator, "CLion") == 0)
    {
        // Call vcvarsall before running cmake for clion builds
        stream.write_fmt(R"("%s" x64 && )", build_info.vcvarsall_path.c_str());
    }

    stream.write_fmt(
        R"("%s" "%s" -G "%s" -B )",
        build_info.cmake_path.c_str(),
        build_info.project_root.c_str(),
        config_info.cmake_generator
    );

    const auto is_single_config = config_info.build_type != nullptr && str::compare(config_info.build_type, "MultiConfig") != 0;

    if (is_single_config)
    {
        stream.write_fmt(R"("%s" )", build_dir.join(config_info.build_type).c_str());
        stream.write_fmt(" -DCMAKE_BUILD_TYPE=%s", config_info.build_type);
        stream.write_fmt(R"(-DCMAKE_INSTALL_PREFIX="%s")", install_dir.join(config_info.build_type).c_str());
    }
    else
    {
        stream.write_fmt(R"("%s" )", build_dir.c_str());
        stream.write_fmt(R"(-DCMAKE_INSTALL_PREFIX="%s")", install_dir.c_str());
    }


    // Add the extra cmake options to pass to the build system
    for (int opt = 0; opt < config_info.cmake_options_count; ++opt)
    {
        stream.write_fmt("%s ", config_info.cmake_options[opt]);
    }

    log_info("Running CMake with command: %s", cmd.c_str());

    bee::ProcessHandle cmake_handle{};
    bee::CreateProcessInfo proc_info{};
    proc_info.handle = &cmake_handle;
    proc_info.flags = bee::CreateProcessFlags::priority_high | bee::CreateProcessFlags::create_hidden;
    proc_info.command_line = cmd.c_str();

    if (!bee::create_process(proc_info))
    {
        bee::log_error("bb: Unable to find cmake");
        return false;
    }

    bee::wait_for_process(cmake_handle);
    bee::destroy_process(cmake_handle);

    return true;
}

int build(const String& cmake_cmd)
{
    return EXIT_SUCCESS;
}

int bb_entry(int argc, char** argv)
{
    FixedHashMap<String, String> generator_mappings =
    {
        { "VS2017", "Visual Studio 15 2017 Win64" },
        { "CLion", "CodeBlocks - NMake Makefiles" }
    };

    String generator_help = "Generator to use when configuring build system.\nAvailable generators (bb => cmake):\n";
    for (const auto& mapping : generator_mappings)
    {
        generator_help += "  - " + mapping.key + " => " + mapping.value + "\n";
    }

    cli::Positional generator_pos("generator", "The project generator to use. Available options: ");
    int cur_generator = 0;
    for (const auto& g : generator_mappings)
    {
        generator_pos.help.append(g.key);
        if (cur_generator < generator_mappings.size() - 1)
        {
            generator_pos.help.append(", ");
        }
        ++cur_generator;
    }

    cli::ParserDescriptor subparsers[2]{};
    subparsers[0].command_name = "configure";
    subparsers[0].positional_count = 1;
    subparsers[0].positionals = &generator_pos;

    subparsers[1].command_name = "build";

    cli::ParserDescriptor parser{};
    parser.subparser_count = bee::static_array_length(subparsers);
    parser.subparsers = subparsers;

    const auto command_line = cli::parse(argc, argv, parser);

    if (!command_line.success)
    {
        log_error("%s", command_line.error_message.c_str());
        return EXIT_FAILURE;
    }

    if (command_line.help_requested)
    {
        log_info("%s", command_line.requested_help_string);
        return EXIT_SUCCESS;
    }

    const auto configure_cmd = command_line.subparsers.find("configure");
    const auto build_cmd = command_line.subparsers.find("build");

    // Handle the `build` subparser
    if (build_cmd != nullptr)
    {
        return build(String());
    }

    // Handle the `configure` subparser
    if (configure_cmd != nullptr)
    {
        const auto generator = cli::get_positional(configure_cmd->value, 0);
        // Project root is always at: <Root>/<BuildConfig>/bb.exe/../../../
        const auto cmake_generator = generator_mappings.find(generator);
        if (cmake_generator == nullptr)
        {
            log_error("Invalid generator specified: %s", generator);
            return EXIT_FAILURE;
        }

        ConfigureInfo config_info{};
        config_info.bb_generator = generator;
        config_info.cmake_generator = cmake_generator->value.c_str();
        config_info.cmake_options_count = cli::get_remainder_count(command_line);
        config_info.cmake_options = cli::get_remainder(command_line);

        DynamicArray<String> build_types;

        if (str::compare(config_info.bb_generator, "CLion") == 0)
        {
            build_types.push_back("Debug");
            build_types.push_back("Release");
        }
        else
        {
            build_types.push_back("MultiConfig");
        }

        for (const auto& build_type : build_types)
        {
            config_info.build_type = build_type.c_str();
            if (!configure(config_info))
            {
                return EXIT_FAILURE;
            }
        }

        return EXIT_SUCCESS;
    }

    log_error("Missing required subparsers");
    log_info("%s", command_line.help_string.c_str());
    return EXIT_SUCCESS;
}


} // namespace bee


int main(int argc, char** argv)
{
    return bee::bb_entry(argc, argv);
}
