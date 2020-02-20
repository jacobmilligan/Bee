/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Main.hpp"
#include "Bee/Core/JSON/JSON.hpp"

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
    bool                    reset_cache { false };
    const char*             bb_generator { nullptr };
    const char*             cmake_generator { nullptr };
    DynamicArray<String>    cmake_options;
};


const BuildInfo& get_build_info()
{
    static BuildInfo info;

    if (info.project_root.empty())
    {
        info.project_root = Path::executable_path().parent().parent().parent();
        info.build_dir = info.project_root.join("Build");

        const auto bin_root = info.project_root.join("ThirdParty/Binaries");
#if BEE_OS_WINDOWS == 1
        info.cmake_path = bin_root.join("cmake/bin/cmake.exe").normalize();

        // Get the path to vcvarsall - this is a complicated process so buckle up...

        // Run cmake in a shell with vcvarsall if the CLion generator is used otherwise NMake won't know where to find VS
        const auto vswhere_location = bin_root.join("vswhere.exe");
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

bool configure(const DynamicArray<String>& build_types, const ConfigureInfo& config_info)
{
    auto cmake_processes = FixedArray<ProcessHandle>::with_size(build_types.size());

    for (const auto build_type_enumer : enumerate(build_types))
    {
        const String& build_type = build_type_enumer.value;
        auto& build_info = get_build_info();
        auto build_dir = build_info.build_dir.join(config_info.bb_generator);
        if (!build_type.empty())
        {
            build_dir.join(build_type.view());
        }

        if (config_info.reset_cache)
        {
            const auto cache_path = build_dir.join("CMakeCache.txt");
            if (fs::is_file(cache_path))
            {
                fs::remove(cache_path);
            }
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

        const auto is_single_config = !build_type.empty() && str::compare(build_type, "MultiConfig") != 0;

        if (is_single_config)
        {
            stream.write_fmt(R"("%s" )", build_dir.join(build_type.view()).c_str());
            stream.write_fmt(" -DCMAKE_BUILD_TYPE=%s", build_type.c_str());
            stream.write_fmt(R"( -DCMAKE_INSTALL_PREFIX="%s" )", install_dir.join(build_type.view()).c_str());
        }
        else
        {
            stream.write_fmt(R"("%s" )", build_dir.c_str());
            stream.write_fmt(R"(-DCMAKE_INSTALL_PREFIX="%s" )", install_dir.c_str());
        }

        // Add the extra cmake options to pass to the build system
        for (const auto& opt : config_info.cmake_options)
        {
            stream.write_fmt("%s ", opt.c_str());
        }

        log_info("\nbb: Configuring %s build with CMake command:\n\n%s\n", build_type.c_str(), cmd.c_str());

        CreateProcessInfo proc_info{};
        proc_info.handle = &cmake_processes[build_type_enumer.index];
        proc_info.flags = CreateProcessFlags::priority_high | CreateProcessFlags::create_hidden;
        proc_info.command_line = cmd.c_str();

        if (!create_process(proc_info))
        {
            log_error("bb: Unable to find cmake");
            return false;
        }
    }

    for (auto& proc_handle : cmake_processes)
    {
        wait_for_process(proc_handle);
        destroy_process(proc_handle);
    }

    return true;
}

int build(const String& cmake_cmd)
{
    return EXIT_SUCCESS;
}


void parse_settings_json(const Path& location, DynamicArray<String>* cmake_options)
{
    if (!location.exists())
    {
        log_error("No settings JSON file exists at that location: %s", location.c_str());
        return;
    }

    auto json_src = fs::read(location);

    json::Document doc(json::ParseOptions{});
    doc.parse(json_src.data());

    const auto options_json = doc.get_member(doc.root(), "cmake_options");
    if (!options_json.is_valid() || doc.get_data(options_json).type != json::ValueType::object)
    {
        log_error("Missing `cmake_options` array in settings JSON root");
        return;
    }

    for (auto opt : doc.get_members_range(options_json))
    {
        const auto data = doc.get_data(opt.value);
        if (data.type != json::ValueType::string)
        {
            log_error("invalid option format - not a string");
            continue;
        }

        cmake_options->push_back(str::format("-D%s=%s", opt.key, data.as_string()));
    }
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

    /*
     * Add all the generator types to the positional arguments help string, i.e.:
     * `The project generator to use. Available options: VS2017, CLion`
     */
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

    cli::Option configure_options[] = {
        { cli::Option('s', "settings", false, "A JSON file containing CMake settings", 1) },
        { cli::Option('r', "reset", false, "Forces a reset of the CMake cache", 0) }
    };

    cli::ParserDescriptor subparsers[2]{};
    subparsers[0].command_name = "configure";
    subparsers[0].positional_count = 1;
    subparsers[0].positionals = &generator_pos;
    subparsers[0].option_count = static_array_length(configure_options);
    subparsers[0].options = configure_options;

    subparsers[1].command_name = "build";
    subparsers[1].option_count = static_array_length(configure_options);
    subparsers[1].options = configure_options;

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
        // EditorApp root is always at: <Root>/<BuildConfig>/bb.exe/../../../
        const auto cmake_generator = generator_mappings.find(generator);
        if (cmake_generator == nullptr)
        {
            log_error("Invalid generator specified: %s", generator);
            return EXIT_FAILURE;
        }

        ConfigureInfo config_info{};
        config_info.reset_cache = cli::has_option(configure_cmd->value, "reset");
        config_info.bb_generator = generator;
        config_info.cmake_generator = cmake_generator->value.c_str();

        const auto settings_file = cli::get_option(configure_cmd->value, "settings");
        if (settings_file != nullptr)
        {
            parse_settings_json(Path::current_working_directory().join(settings_file), &config_info.cmake_options);
        }

        const auto remainder_count = cli::get_remainder_count(command_line);
        const auto remainder = cli::get_remainder(command_line);
        for (int i = 0; i < remainder_count; ++i)
        {
            config_info.cmake_options.push_back(String(remainder[i]));
        }

        DynamicArray<String> build_types;

        if (str::compare(config_info.bb_generator, "CLion") == 0)
        {
            build_types.push_back("Debug");
            build_types.push_back("Release");
            config_info.bb_generator = "CLion";
        }
        else
        {
            build_types.push_back("MultiConfig");
        }

        // Configure the standard build types
        if (!configure(build_types, config_info))
        {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    log_error("Missing required subparsers");
    log_info("%s", command_line.help_string.c_str());
    return EXIT_SUCCESS;
}


} // namespace bee


int bee_main(int argc, char** argv)
{
    return bee::bb_entry(argc, argv);
}
