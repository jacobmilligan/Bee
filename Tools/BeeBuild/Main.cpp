/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "BeeBuild/Environment.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/JSON/JSON.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Time.hpp"

namespace bee {


enum class CMakeGenerator
{
    visual_studio_15_2017_win64,
    visual_studio_16_2019_win64,
    codeblocks_ninja,
    codeblocks_nmake_makefiles,
    unknown
};

enum class BuildType
{
    debug,
    release,
    multi_config,
    unknown
};

BEE_TRANSLATION_TABLE_FUNC(get_cmake_generator_string, CMakeGenerator, const char*, CMakeGenerator::unknown,
    "Visual Studio 15 2017 Win64",  // visual_studio_15_2017_win64
    "Visual Studio 16 2019",        // visual_studio_16_2019_win64
    "CodeBlocks - Ninja",           // ninja
    "CodeBlocks - NMake Makefiles"  // nmake
)

BEE_TRANSLATION_TABLE_FUNC(get_build_type_string, BuildType, const char*, BuildType::unknown,
    "Debug",        // debug
    "Release",      // release
    "MultiConfig"
)

BEE_TRANSLATION_TABLE_FUNC(get_extra_cmake_args, CMakeGenerator, const char*, CMakeGenerator::unknown,
    nullptr,        // visual_studio_15_2017_win64
    "-A x64",       // visual_studio_16_2019_win64
    nullptr,        // ninja
    nullptr         // nmake
)

struct GeneratorInfo
{
    BuildIDE        ide {BuildIDE::unknown };
    CMakeGenerator  cmake { CMakeGenerator::unknown };
};

struct ConfigureInfo
{
    const BuildEnvironment* environment { nullptr };
    const GeneratorInfo*    generator_info { nullptr };
    bool                    reset_cache { false };
    DynamicArray<String>    cmake_options;
    DynamicArray<BuildType> build_types;
};

/*
 * Mappings from bee generator type to cmake generator - lookup using string key via
 * find_generator
 */
static constexpr GeneratorInfo g_generators[] = {
    { BuildIDE::vs2017, CMakeGenerator::visual_studio_15_2017_win64 },
    { BuildIDE::vs2019, CMakeGenerator::visual_studio_16_2019_win64 },
    { BuildIDE::clion,  CMakeGenerator::codeblocks_nmake_makefiles }
};

GeneratorInfo find_generator(const char* name)
{
    const auto index = find_index_if(g_generators, [&](const GeneratorInfo& info)
    {
        return str::compare(name, to_string(info.ide)) == 0;
    });

    return index >= 0 ? g_generators[index] : GeneratorInfo{};
}

bool configure(const ConfigureInfo& config_info)
{
    auto cmake_processes = FixedArray<ProcessHandle>::with_size(config_info.build_types.size());
    const auto& generator_info = *config_info.generator_info;

    for (const auto build_type_enumer : enumerate(config_info.build_types))
    {
        const BuildType build_type = build_type_enumer.value;
        const auto* build_type_string = get_build_type_string(build_type);
        const auto* bb_generator_string = to_string(generator_info.ide);
        const auto* cmake_generator_string = get_cmake_generator_string(generator_info.cmake);
        auto output_directory = config_info.environment->build_dir.join(bb_generator_string);

        if (build_type != BuildType::unknown && build_type != BuildType::multi_config)
        {
            // we only want to output to subdirs if we have to generate i.e. seperate makefiles
            output_directory.append(build_type_string);
        }

        if (config_info.reset_cache)
        {
            const auto cache_path = output_directory.join("CMakeCache.txt");
            if (fs::is_file(cache_path.view()))
            {
                fs::remove(cache_path.view());
            }
        }

        const auto install_dir = output_directory.join("Install");

        String cmd;
        io::StringStream stream(&cmd);

        if (generator_info.ide == BuildIDE::clion)
        {
            // Call vcvarsall before running cmake for clion builds
            const auto* env = config_info.environment;
            const auto& vsvarsall = env->windows.vcvarsall_path[env->windows.default_ide];
            stream.write_fmt(R"("%s" x64 && )", vsvarsall.c_str());
        }

        // build the cmake.exe command
        stream.write_fmt(
            R"("%s" "%s" -G "%s" )",
            config_info.environment->cmake_path.c_str(),
            config_info.environment->project_root.c_str(),
            cmake_generator_string
        );

        const char* extra_args = get_extra_cmake_args(generator_info.cmake);
        if (extra_args != nullptr)
        {
            stream.write_fmt("%s ", extra_args);
        }

        if (build_type != BuildType::multi_config)
        {
            stream.write_fmt(R"(-B "%s" )", output_directory.c_str());
            stream.write_fmt(" -DCMAKE_BUILD_TYPE=%s", build_type_string);
            stream.write_fmt(R"( -DCMAKE_INSTALL_PREFIX="%s" )", install_dir.join(build_type_string).c_str());
        }
        else
        {
            stream.write_fmt(R"(-B "%s" )", output_directory.c_str());
            stream.write_fmt(R"(-DCMAKE_INSTALL_PREFIX="%s" )", install_dir.c_str());
        }

        // Add the extra cmake options to pass to the build system
        for (const auto& opt : config_info.cmake_options)
        {
            stream.write_fmt("%s ", opt.c_str());
        }

        log_info("\nbb: Configuring %s build with CMake command:\n\n%s\n", get_build_type_string(build_type), cmd.c_str());

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

int prepare_plugin(const BuildEnvironment& env, const Path& lib_path)
{
    if (env.platform == BuildPlatform::windows)
    {
        if (!lib_path.exists() || lib_path.extension() != ".dll")
        {
            log_error("Skipping hot-reload preperation: no dll found at %s", lib_path.c_str());
            return EXIT_SUCCESS;
        }

        auto pdb_path = lib_path;
        pdb_path.set_extension(".pdb");

        if (!pdb_path.exists())
        {
            log_error("Skipping hot-reload preperation: no PDB found at %s", pdb_path.c_str());
            return EXIT_SUCCESS;
        }

        const auto timestamp = time::now();
        auto random_pdb_path = lib_path;
        random_pdb_path.set_extension(str::to_string(timestamp, temp_allocator()).view())
                       .append_extension(".pdb");

        fs::move(pdb_path.view(), random_pdb_path.view());
        fs::copy(random_pdb_path.view(), pdb_path.view());

        log_info("Prepared plugin %s for hot reloading", lib_path.c_str());
    }

    return EXIT_SUCCESS;
}


void parse_settings_json(const Path& location, DynamicArray<String>* cmake_options)
{
    if (!location.exists())
    {
        log_error("No settings JSON file exists at that location: %s", location.c_str());
        return;
    }

    auto json_src = fs::read_all_text(location.view());

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
    cli::Positional generator_pos("generator", "The project generator to use. Available options: ");
    cli::Positional lib_path_pos("lib-path", "Absolute path to the plugins .dll/.so/.dylib file to prepare");

    /*
     * Add all the generator types to the positional arguments help string, i.e.:
     * `The project generator to use. Available options: VS2017, CLion`
     */
    io::StringStream stream(&generator_pos.help);

    stream.write("  Generator to use when configuring build system.\n  Available generators (bb => cmake):\n");

    for (const auto generator : enumerate(g_generators))
    {
        const auto* bb_name = to_string(generator.value.ide);
        const auto* cmake_name = get_cmake_generator_string(generator.value.cmake);

        stream.write_fmt("   - %s => %s", bb_name, cmake_name);

        if (generator.index < static_array_length(g_generators) - 1)
        {
            stream.write_fmt("\n");
        }
    }

    cli::Option configure_options[] = {
        { cli::Option('s', "settings", false, "A JSON file containing CMake settings", 1) },
        { cli::Option('r', "reset", false, "Forces a reset of the CMake cache", 0) }
    };

    cli::ParserDescriptor subparsers[3];
    subparsers[0].command_name = "configure";
    subparsers[0].positional_count = 1;
    subparsers[0].positionals = &generator_pos;
    subparsers[0].option_count = static_array_length(configure_options);
    subparsers[0].options = configure_options;

    subparsers[1].command_name = "build";
    subparsers[1].option_count = static_array_length(configure_options);
    subparsers[1].options = configure_options;

    subparsers[2].command_name = "prepare-plugin";
    subparsers[2].positional_count = 1;
    subparsers[2].positionals = &lib_path_pos;

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

    BuildEnvironment build_environment{};
    init_build_environment(&build_environment);

    const auto* configure_cmd = command_line.subparsers.find("configure");
    const auto* build_cmd = command_line.subparsers.find("build");
    const auto* prepare_plugin_cmd = command_line.subparsers.find("prepare-plugin");

    // Handle the prepare-plugin subparser
    if (prepare_plugin_cmd != nullptr)
    {
        return prepare_plugin(build_environment, cli::get_positional(prepare_plugin_cmd->value, 0));
    }

    // Handle the `build` subparser
    if (build_cmd != nullptr)
    {
        return build(String());
    }

    // Handle the `configure` subparser
    if (configure_cmd != nullptr)
    {
        const auto generator_info = find_generator(cli::get_positional(configure_cmd->value, 0));

        if (generator_info.ide == BuildIDE::unknown)
        {
            log_error("Invalid generator specified: %s", cli::get_positional(configure_cmd->value, 0));
            return EXIT_FAILURE;
        }

        ConfigureInfo config_info{};
        config_info.environment = &build_environment;
        config_info.generator_info = &generator_info;
        config_info.reset_cache = cli::has_option(configure_cmd->value, "reset");

        // Parse the settings file if specified
        if (cli::has_option(configure_cmd->value, "settings"))
        {
            const auto* settings_file = cli::get_option(configure_cmd->value, "settings");
            parse_settings_json(Path(current_working_directory()).append(settings_file), &config_info.cmake_options);
        }

        // Collect cmake arguments from the remainder after the '--' at the command line
        const auto remainder_count = cli::get_remainder_count(command_line);
        const auto* remainder = cli::get_remainder(command_line);
        for (int i = 0; i < remainder_count; ++i)
        {
            config_info.cmake_options.push_back(String(remainder[i]));
        }


        if (generator_info.ide == BuildIDE::clion)
        {
            // CLion isn't multiconfig
            config_info.build_types.push_back(BuildType::debug);
            config_info.build_types.push_back(BuildType::release);

            if (build_environment.platform == BuildPlatform::windows && generator_info.cmake == CMakeGenerator::codeblocks_ninja)
            {
                // We need to make sure with CLion that ninja chooses msvc
                String env_path_var(temp_allocator());
                get_environment_variable("Path", &env_path_var);

                DynamicArray<StringView> path_entries(temp_allocator());
                str::split(env_path_var.view(), &path_entries, environment_path_delimiter);

                String cmake_ignore_path;

                // iterate all the entries in PATH and ignore any containing a mingw directory (ignore GCC)
                for (const auto& entry : path_entries)
                {
                    if (str::first_index_of(entry, "mingw64") < 0 && str::first_index_of(entry, "mingw32") < 0)
                    {
                        continue;
                    }

                    if (!cmake_ignore_path.empty())
                    {
                        cmake_ignore_path += ",";
                    }

                    cmake_ignore_path += entry;
                }

                // add CMAKE_IGNORE_PATH to ensure Ninja selects visual studio
                if (!cmake_ignore_path.empty())
                {
                    config_info.cmake_options.push_back("-DCMAKE_IGNORE_PATH=" + cmake_ignore_path);
                }
            }
        }
        else
        {
            config_info.build_types.push_back(BuildType::multi_config);
        }

        // Configure the standard build types
        if (!configure(config_info))
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
