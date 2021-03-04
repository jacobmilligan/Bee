/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "CodeGen.hpp"
#include "Frontend.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/IO.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    BEE_DISABLE_WARNING_MSVC(4996)
    #include <clang/Tooling/CommonOptionsParser.h>
    #include <clang/Tooling/Tooling.h>
    #include <llvm/Support/CommandLine.h>
BEE_POP_WARNING


int bee_main(int argc, char** argv)
{
    bee::reflection_register_builtin_types();

    // Set up the command line options
    llvm::cl::OptionCategory bee_reflect_cat("bee-reflect options");

    llvm::cl::opt<std::string> output_dir_opt(
        "output",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Directory to output all generated cpp files"),
        llvm::cl::Required
    );
    llvm::cl::opt<bool> inline_opt(
        "inline",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Generate reflection as a .inl file to be #included rather than a .cpp file with exported symbols")
    );
    llvm::cl::alias inline_alias("i", llvm::cl::desc("Alias for -inline"), llvm::cl::aliasopt(inline_opt));
    llvm::cl::opt<bool> dump_opt(
        "dump-command",
        llvm::cl::cat(bee_reflect_cat),
        llvm::cl::desc("Dump the command line used to invoke bee-reflect to a file in the <output> directory")
    );

    const int unudjusted_argc = argc;

    // CommonOptionsParser declares HelpMessage with a description of the common
    // command-line options related to the compilation database and input files.
    // It's nice to have this help message in all tools.
    llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
    clang::tooling::CommonOptionsParser options_parser(argc, const_cast<const char**>(argv), bee_reflect_cat);

    clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

    const auto output_dir = bee::PathView(output_dir_opt.c_str());
    const auto generated_inl_dir = bee::Path(output_dir).append("ReflectedTemplates");

    if (!output_dir.exists())
    {
        bee::fs::mkdir(output_dir, true);
    }

    if (!generated_inl_dir.exists())
    {
        bee::fs::mkdir(generated_inl_dir.view(), true);
    }

    // dump command line if requested before doing anything else in case of assert
    if (dump_opt)
    {
        auto command_out = bee::Path(output_dir).append("command.txt");
        auto file = bee::fs::open_file(command_out.view(), bee::fs::OpenMode::write);
        bee::io::FileStream filestream(&file);
        for (int i = 1; i < unudjusted_argc; ++i)
        {
            filestream.write(argv[i]);
            filestream.write("\n");
        }
    }

    bee::reflect::BeeReflectFrontendActionFactory factory;

    const auto result = tool.run(&factory);

    if (result != 0)
    {
        bee::log_error("bee-reflect: failed to generate reflection data");
        return result;
    }

    // Keep track of all the reflected files absolute paths for later so we can delete old, nonreflected files
    bee::DynamicArray<bee::PathView> reflected_abs_paths;
    bee::DynamicArray<bee::reflect::TypeListEntry> reflected_types;

    const auto src_path_list = options_parser.getSourcePathList();
    int type_count_for_inl = 0;

    // Output a .generated.cpp file for each of the reflected headers
    for (auto& file : factory.storage.reflected_files)
    {
        const auto iter = std::find_if(src_path_list.begin(), src_path_list.end(), [&](const std::string& str)
        {
            return llvm::StringRef(str).endswith(llvm::StringRef(file.value.location.c_str(), file.value.location.size()));
        });

        if (iter == src_path_list.end())
        {
            continue;
        }

        auto src_codegen_mode = bee::reflect::CodegenMode::cpp;
        if (inline_opt)
        {
            src_codegen_mode = bee::reflect::CodegenMode::inl;
        }

        auto output_path = bee::Path(output_dir, bee::temp_allocator())
            .append(file.value.location.filename())
            .set_extension("generated")
            .append_extension(".cpp");

        // Generate all non-templated types into a generated.cpp file
        bee::String output;
        if (bee::reflect::generate_reflection(output_path.view(), file.value, &output, src_codegen_mode) <= 0)
        {
            // If there's only template types in a generated file, this should be re-written as a empty file
            output.clear();
            bee::reflect::generate_empty_reflection(output_path.view(), file.value.location.c_str(), &output);
        }
        bee::fs::write_all(output_path.view(), output.view());

        // Generate a matching .inl file with just the get_type(module, index) portion if inline mode and the file has
        // non-template types
        if (src_codegen_mode == bee::reflect::CodegenMode::inl)
        {
            output_path.set_extension(".inl");
            output.clear();

            type_count_for_inl += bee::reflect::generate_reflection_header(
                output_path.view(),
                file.value,
                type_count_for_inl,
                &output,
                src_codegen_mode
            );

            bee::fs::write_all(output_path.view(), output.view());
        }

        // Output a generated.inl file if required - a type in the file is templated and requires a `get_type` specialization
        const auto inl_path = bee::Path(generated_inl_dir.view(), bee::temp_allocator())
            .append(file.value.location.filename())
            .set_extension("generated")
            .append_extension("inl");
        output.clear();
        if (bee::reflect::generate_reflection(inl_path.view(), file.value, &output, bee::reflect::CodegenMode::templates_only) > 0)
        {
            bee::fs::write_all(inl_path.view(), output.view());
        }

        reflected_abs_paths.push_back(file.value.location.view());
        reflected_types.append(file.value.all_types.const_span()); // keep track of these for generating typelists
    }

    bee::String header_comment(bee::temp_allocator());

    for (const std::string& compilation : options_parser.getSourcePathList())
    {
        const auto was_reflected = bee::find_index_if(reflected_abs_paths, [&](const bee::PathView& reflected)
        {
            return compilation.c_str() == reflected;
        }) >= 0;

        if (!was_reflected)
        {
            const auto filename = bee::Path(compilation.c_str(), bee::temp_allocator()).filename();
            auto output_path = bee::Path(output_dir, bee::temp_allocator())
                .append(filename)
                .set_extension("generated")
                .append_extension("cpp");
            bee::reflect::generate_empty_reflection(output_path.view(), compilation.c_str(), &header_comment);

            bee::fs::write_all(output_path.view(), header_comment.view());
            header_comment.clear();
        }
    }

    const auto typelist_mode = inline_opt ? bee::reflect::CodegenMode::inl : bee::reflect::CodegenMode::cpp;
    bee::reflect::generate_typelist(output_dir, reflected_types.const_span(), typelist_mode, reflected_abs_paths.const_span());

    return EXIT_SUCCESS;
}