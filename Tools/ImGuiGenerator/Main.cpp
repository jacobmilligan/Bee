/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "ImGuiGenerator/Generator.inl"

#include "BeeBuild/Environment.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include <algorithm>

namespace bee {


struct ImGuiConfig
{
    const char* module_name { nullptr };
    const char* output_path { nullptr };
    const char* ns { nullptr };
    const char* user_config { nullptr };
    bool        generate_internal { false };
    BEE_PAD(7);
};


static int generate_imgui(const ImGuiConfig& config)
{
    const auto full_output_path = Path(current_working_directory()).append(config.output_path).normalize();
    const auto full_output_dir = full_output_path.parent();
    if (!full_output_dir.parent().exists())
    {
        log_error("Folder does not exist: %" BEE_PRIsv, BEE_FMT_SV(full_output_dir.parent()));
        return EXIT_FAILURE;
    }

    log_info("Outputting to: %s", full_output_path.c_str());

    BuildEnvironment env{};
    if (!init_build_environment(&env))
    {
        log_error("Failed to initialize build environment");
        return EXIT_FAILURE;
    }

    const auto tool_root = env.project_root.join("Tools/ImGuiGenerator").make_preferred();
    String command_line(temp_allocator());
    io::StringStream cmd_formatter(&command_line);

    if (env.platform == BuildPlatform::windows)
    {

        cmd_formatter.write_fmt(
            R"("%s" x64 && %s /c "%s\\generator.bat cl)",
            env.windows.vcvarsall_path[env.windows.default_ide].c_str(),
            env.windows.comspec_path.c_str(),
            tool_root.c_str()
        );
    }
    else
    {
        log_error("Platform not supported");
        return EXIT_FAILURE;
    }

    if (config.generate_internal)
    {
        command_line.append(" internal");
    }

    if (env.platform == BuildPlatform::windows)
    {
        command_line.append(R"(")");
    }

    log_info("%s", command_line.c_str());

    // run the cimgui generator script before post-processing the output
    ProcessHandle cimgui{};
    CreateProcessInfo proc_info{};
    proc_info.handle = &cimgui;
    proc_info.flags = CreateProcessFlags::priority_high;
    proc_info.command_line = command_line.c_str();

    if (!create_process(proc_info, tool_root.view()))
    {
        log_error("Failed to execute cimgui generator");
        return EXIT_FAILURE;
    }

    // run cimgui generator
    wait_for_process(cimgui);
    const int cimgui_exit_code = get_process_exit_code(cimgui);
    destroy_process(cimgui);

    if (cimgui_exit_code != 0)
    {
        log_error("cimgui failed to generate code successfully");
        return EXIT_FAILURE;
    }

    const auto cimgui_generator_root = env.project_root.join("ThirdParty/cimgui/generator");
    auto definitions_contents = fs::read_all_text(cimgui_generator_root.join("output/definitions.json").view(), temp_allocator());

    JSONSerializer serializer(definitions_contents.data(), JSONSerializeFlags::parse_in_situ, temp_allocator());
    DynamicHashMap<String, DynamicArray<Definition>> definitions;
    serialize(SerializerMode::reading, SerializerSourceFlags::all, &serializer, &definitions, temp_allocator());

    DynamicArray<Definition> sorted_definitions;
    for (auto& def : definitions)
    {
        for (auto& ov : def.value)
        {
            if (str::first_index_of(ov.get_name().view(), "ig") == 0)
            {
                ov.plugin_name = str::substring(ov.get_name().view(), 2);
            }
            else
            {
                ov.plugin_name = ov.get_name();
            }
            sorted_definitions.emplace_back(BEE_MOVE(ov));
        }
    }
    std::sort(sorted_definitions.begin(), sorted_definitions.end(), [](const Definition& lhs, const Definition& rhs)
    {
        return lhs.get_name() < rhs.get_name();
    });

    if (!full_output_dir.exists())
    {
        fs::mkdir(full_output_dir);
    }

    // Output the header/impl as a single split header file
    String generated_contents;
    io::StringStream stream(&generated_contents);

    // Copy over the struct and enum definitions straight from the generated cimgui header
    auto cimgui_header = fs::read_all_text(cimgui_generator_root.join("../cimgui.h").view());
    // replace windows line breaks
    str::replace(&cimgui_header, "\r\n", "\n");
    // ensure the formatting of the endif comments are consistent
    str::replace(&cimgui_header, "#endif //CIMGUI_DEFINE_ENUMS_AND_STRUCTS", "#endif // CIMGUI_DEFINE_ENUMS_AND_STRUCTS");

    // get the struct and enum definition substring - occurs between the first macro pair (we need to grab the
    // template typedefs defined between the second ifdef pair)
    const auto types_begin = str::first_index_of(cimgui_header, "#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS");
    const auto types_size = str::first_index_of(cimgui_header, "#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS") - types_begin;

    // remove all the macro guards as we don't need them
    String enums_and_structs(str::substring(cimgui_header, types_begin, types_size), temp_allocator());
    str::replace(&enums_and_structs, "#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS", "#ifndef BEE_IMGUI_GENERATOR_IMPLEMENTATION");
    str::replace(&enums_and_structs, "#endif // CIMGUI_DEFINE_ENUMS_AND_STRUCTS", "#endif // BEE_IMGUI_GENERATOR_IMPLEMENTATION");
    str::replace(&enums_and_structs, "#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS", "");
    str::replace(&enums_and_structs, "#else\nstruct GLFWwindow;\nstruct SDL_Window;\ntypedef union SDL_Event SDL_Event;", "");
    str::trim(&enums_and_structs, '\n');

    // write out a header comment
    stream.write_fmt(R"(/*
*  This file was generated by the bee-imgui-generator tool. DO NOT EDIT DIRECTLY.
*
*  Generated on: %s
*/

#include <stdio.h>
#include <stdint.h>

#ifdef BEE_IMGUI_GENERATOR_IMPLEMENTATION
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#endif // BEE_IMGUI_GENERATOR_IMPLEMENTATION

)", get_local_unix_timestamp());

    // Output the ImGui structs and enums - NOT ENCLOSED IN NAMESPACE
    stream.write(enums_and_structs.view());

    // Output the API struct - MAY BE ENCLOSED IN NAMESPACE
    if (config.ns != nullptr)
    {
        stream.write_fmt("\n\nnamespace %s {\n\n\n", config.ns);
    }
    stream.write_fmt("struct %s\n{\n", config.module_name);

    // write out the function pointers in the API struct
    for (const auto& overload : sorted_definitions)
    {
        if (overload.templated)
        {
            continue;
        }

        if (overload.constructor && !overload.stname.empty())
        {
            stream.write_fmt("    %s* (*%s)%s { nullptr };\n", overload.stname.c_str(), overload.plugin_name.c_str(), overload.args.c_str());
        }
        else
        {
            stream.write_fmt("    %s (*%s)%s { nullptr };\n", overload.ret.c_str(), overload.plugin_name.c_str(), overload.args.c_str());
        }
    }
    stream.write_fmt("}; // struct %s\n\n", config.module_name);

    if (config.ns != nullptr)
    {
        stream.write_fmt("\n} // namespace %s\n\n", config.ns);
    }

    // write out the implementation section defining a single function loading the function pointers into the API struct
    stream.write("#ifdef BEE_IMGUI_GENERATOR_IMPLEMENTATION\n");
    if (config.ns != nullptr)
    {
        stream.write_fmt("void bee_load_imgui_api(%s::%s* api)\n{\n", config.ns, config.module_name);
    }
    else
    {
        stream.write_fmt("void bee_load_imgui_api(%s* api)\n{\n", config.module_name);
    }
    for (const auto& overload : sorted_definitions)
    {
        if (overload.templated)
        {
            continue;
        }
        stream.write_fmt("    api->%s = %s;\n", overload.plugin_name.c_str(), overload.get_name().c_str());
    }
    stream.write("} // bee_load_imgui_api\n");
    stream.write("#endif // BEE_IMGUI_GENERATOR_IMPLEMENTATION\n");

    fs::write_all(full_output_path.view(), generated_contents.view());

    return EXIT_SUCCESS;
}


} // namespace bee


int bee_main(int argc, char** argv)
{
    bee::cli::Positional positionals[] = {
        bee::cli::Positional("module-name", "Name to give the module struct"),
        bee::cli::Positional("output-path", "Path to output the generated imgui interface file to")
    };

    bee::cli::Option options[] = {
        bee::cli::Option('i', "--internal", false, "Generate API for imgui_internal.h", 0),
        bee::cli::Option('n', "--namespace", false, "Enclose the API struct in a namespace", 1),
        bee::cli::Option('u', "--user-config", false, "Path to user config", 1)
    };

    bee::cli::ParserDescriptor cli_desc{};
    cli_desc.positionals = positionals;
    cli_desc.positional_count = bee::static_array_length(positionals);
    cli_desc.options = options;
    cli_desc.option_count = bee::static_array_length(options);
    const auto command_line = bee::cli::parse(argc, argv, cli_desc);

    if (command_line.help_requested)
    {
        bee::log_info("%s", command_line.requested_help_string);
        return EXIT_SUCCESS;
    }

    if (!command_line.success)
    {
        bee::log_error("%s", command_line.error_message.c_str());
        return EXIT_FAILURE;
    }

    bee::ImGuiConfig config{};
    config.module_name = bee::cli::get_positional(command_line, 0);
    config.output_path = bee::cli::get_positional(command_line, 1);
    config.generate_internal = bee::cli::has_option(command_line, "--internal");
    if (bee::cli::has_option(command_line, "--namespace"))
    {
        config.ns = bee::cli::get_option(command_line, "--namespace");
    }
    if (bee::cli::has_option(command_line, "--user-config"))
    {
        config.user_config = bee::cli::get_option(command_line, "--user-config");
    }

    return bee::generate_imgui(config);
}