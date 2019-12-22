/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


//#include "ClangParser.hpp"
//
#include "Frontend.hpp"
#include "CodeGen.hpp"

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Application/Main.hpp"

#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <unordered_set>


int bee_main(int argc, char** argv)
{
    bee::reflection_register_builtin_types();

    // Set up the command line options
    llvm::cl::OptionCategory bee_reflect_cat("bee-reflect options");

    llvm::cl::SubCommand generate_subcommand(
        "generate",
        "Generates static reflection data as a collection .cpp files"
    );
    llvm::cl::opt<std::string> output_dir_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Directory to output all generated cpp files"),
        llvm::cl::Required,
        llvm::cl::sub(generate_subcommand)
    );

    llvm::cl::SubCommand link_subcommand(
        "link",
        "Links .registration files together into a single .cpp file for runtime type registration"
    );
    llvm::cl::opt<std::string> output_link_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Path to write the .cpp result to"),
        llvm::cl::Required,
        llvm::cl::sub(link_subcommand)
    );

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, const_cast<const char**>(argv), bee_reflect_cat);

    if (generate_subcommand)
    {
        clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

        const auto output_dir = bee::Path(output_dir_opt.c_str());
        const auto generated_inl_dir = output_dir.join("ReflectedTemplates");

        if (!output_dir.exists())
        {
            bee::fs::mkdir(output_dir);
        }

        if (!generated_inl_dir.exists())
        {
            bee::fs::mkdir(generated_inl_dir);
        }

        bee::reflect::BeeReflectFrontendActionFactory factory;

        const auto result = tool.run(&factory);

        if (result != 0)
        {
            bee::log_error("bee-reflect: failed to generate reflection data");
            return result;
        }

        // Output a .generated.cpp file for each of the reflected headers
        for (auto& file : factory.storage.reflected_files)
        {
            const auto is_external = bee::container_index_of(options_parser.getSourcePathList(), [&](const std::string& str)
            {
                return llvm::StringRef(str).endswith(llvm::StringRef(file.value.location.c_str(), file.value.location.size()));
            }) < 0;

            if (is_external)
            {
                continue;
            }

            // Generate all non-templated types into a generated.cpp file
            bee::String output;
            bee::io::StringStream stream(&output);
            if (bee::reflect::generate_reflection(file.value, &stream, bee::reflect::CodegenMode::skip_templates) <= 0)
            {
                // If there's only template types in a generated file, this should be re-written as a empty file
                output.clear();
                stream.seek(0, bee::io::SeekOrigin::begin);
                bee::reflect::generate_empty_reflection(file.value.location.c_str(), &stream);
            }

            auto output_path = output_dir.join(file.value.location.filename(), bee::temp_allocator())
                                         .set_extension("generated")
                                         .append_extension("cpp");
            bee::fs::write(output_path, output.view());

            output.clear();
            stream.seek(0, bee::io::SeekOrigin::begin);

            // Output a .registration file for looking up a type by hash
            const auto reg_path = output_dir.join(file.value.location.filename(), bee::temp_allocator()).set_extension("registration");
            bee::reflect::generate_registration(file.value.location, file.value.all_types.span(), &stream);

            bee::fs::write(reg_path, output.view());

            // Output a generated.inl file if required - a type in the file is templated and requires a `get_type` specialization
            output.clear();
            stream.seek(0, bee::io::SeekOrigin::begin);
            if (bee::reflect::generate_reflection(file.value, &stream, bee::reflect::CodegenMode::templates_only) > 0)
            {
                const auto inl_path = generated_inl_dir.join(file.value.location.filename(), bee::temp_allocator())
                    .set_extension("generated")
                    .append_extension("inl");

                bee::fs::write(inl_path, output.view());
            }
        }

        bee::String header_comment(bee::temp_allocator());
        bee::io::StringStream stream(&header_comment);

        for (const std::string& compilation : options_parser.getSourcePathList())
        {
            const auto filename = bee::Path(compilation.c_str(), bee::temp_allocator()).filename();
            const auto path = output_dir.join(filename, bee::temp_allocator()).set_extension("generated").append_extension("cpp");

            if (!path.exists())
            {
                bee::reflect::generate_empty_reflection(compilation.c_str(), &stream);
                bee::fs::write(path, header_comment.view());
                stream.seek(0, bee::io::SeekOrigin::begin);
                header_comment.clear();
            }
        }

        return EXIT_SUCCESS;
    }

    if (link_subcommand)
    {
        auto& source_path_list = options_parser.getSourcePathList();
        auto search_paths = bee::FixedArray<bee::Path>::with_size(source_path_list.size());
        for (int p = 0; p < search_paths.size(); ++p)
        {
            auto path = bee::Path(source_path_list[p].c_str());
            if (!bee::fs::is_dir(path))
            {
                bee::log_error("%s is not a .registration file search path", path.c_str());
                continue;
            }
            search_paths[p] = path;
        }

        bee::String output;
        bee::io::StringStream stream(&output);
        bee::reflect::link_registrations(search_paths.const_span(), &stream);

        bee::Path output_path(output_link_opt.getValue().c_str());
        bee::fs::write(output_path, stream.view());
    }

    return EXIT_SUCCESS;
}